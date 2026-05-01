"""Unit tests for the REST contract drift detector.

Run with:
    python3 -m unittest discover firmware-v3/tools/contract-regen/tests

These tests use only stdlib + PyYAML, in line with the tool itself.
British English in comments and strings — see CLAUDE.md hard constraints.
"""

from __future__ import annotations

import io
import os
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

# Make the tool importable when running unittest from the repo root.
_TOOL_DIR = Path(__file__).resolve().parent.parent
if str(_TOOL_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOL_DIR))

import regen_rest  # noqa: E402  (path injected above)


# ---------------------------------------------------------------------------
# Fixtures — minimal V1ApiRoutes.cpp / contract YAML samples
# ---------------------------------------------------------------------------

SAMPLE_CPP = textwrap.dedent(
    r"""
    // Stub V1ApiRoutes.cpp — exercises every parser branch.

    void registerRoutes(HttpRouteRegistry& registry) {
        // Plain registrations
        registry.onGet("/api/v1/ping", [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{}");
        });
        registry.onPost("/api/v1/effects/set",
            [ctx](AsyncWebServerRequest* request) { /* ... */ });
        registry.onPost("/api/v1/effects/parameters",
            [ctx](AsyncWebServerRequest* request) { /* ... */ });
        registry.onPatch("/api/v1/effects/parameters",
            [ctx](AsyncWebServerRequest* request) { /* ... */ });
        registry.onPut("/api/v1/audio/agc",
            [ctx](AsyncWebServerRequest* request) { /* ... */ });
        registry.onDelete("/api/v1/presets/scratch",
            [ctx](AsyncWebServerRequest* request) { /* ... */ });

        // A regex-based registration with a numeric capture — should normalise to {id}
        registry.onGetRegex("^\\/api\\/v1\\/zones\\/([0-3])$",
            [ctx](AsyncWebServerRequest* request) { /* ... */ });
        registry.onPostRegex("^\\/api\\/v1\\/zones\\/([0-3])\\/effect$",
            [ctx](AsyncWebServerRequest* request) { /* ... */ });

        // A regex-based registration with a non-numeric capture — should normalise to {name}
        registry.onPutRegex("^\\/api\\/v1\\/presets\\/([^/]+)$",
            [ctx](AsyncWebServerRequest* request) { /* ... */ });

        // A line that is a comment — must NOT be treated as a registration.
        // registry.onGet("/api/v1/should-be-ignored", ...);
    }
    """
).strip()


SAMPLE_YAML_MATCHED = textwrap.dedent(
    """
    version: "1.0"
    endpoints:
      /api/v1/ping:
        method: GET
      /api/v1/effects/set:
        method: POST
      /api/v1/effects/parameters:
        method: [POST, PATCH]
      /api/v1/audio/agc:
        method: PUT
      /api/v1/presets/scratch:
        method: DELETE
      /api/v1/zones/{id}:
        method: GET
      /api/v1/zones/{id}/effect:
        method: POST
      /api/v1/presets/{name}:
        method: PUT
    """
).strip()


SAMPLE_YAML_DRIFTED = textwrap.dedent(
    """
    version: "1.0"
    endpoints:
      /api/v1/ping:
        method: GET
      # /api/v1/effects/set is missing here — firmware-only drift
      /api/v1/effects/parameters:
        method: [POST, PATCH]
      /api/v1/audio/agc:
        method: PUT
      /api/v1/presets/scratch:
        method: DELETE
      /api/v1/zones/{id}:
        method: GET
      /api/v1/zones/{id}/effect:
        method: POST
      /api/v1/presets/{name}:
        method: PUT
      # Contract claims an endpoint that the firmware never registers
      /api/v1/legacy/ghost-endpoint:
        method: GET
    """
).strip()


def _write_tmp(name: str, body: str, tmpdir: str) -> str:
    path = os.path.join(tmpdir, name)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(body)
    return path


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------


class ParseCppTest(unittest.TestCase):
    """Case 1 — parser extracts plain + regex registrations correctly."""

    def test_parses_sample_cpp(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cpp = _write_tmp("V1ApiRoutes.cpp", SAMPLE_CPP, tmp)
            routes = regen_rest.parse_cpp_routes(cpp)

        # We expect 9 distinct (path, method) pairs from the fixture.
        self.assertEqual(len(routes), 9, msg=f"unexpected routes: {routes}")

        as_set = {(r.path, r.method) for r in routes}
        self.assertIn(("/api/v1/ping", "GET"), as_set)
        self.assertIn(("/api/v1/effects/set", "POST"), as_set)
        self.assertIn(("/api/v1/effects/parameters", "POST"), as_set)
        self.assertIn(("/api/v1/effects/parameters", "PATCH"), as_set)
        self.assertIn(("/api/v1/audio/agc", "PUT"), as_set)
        self.assertIn(("/api/v1/presets/scratch", "DELETE"), as_set)
        # Regex-derived, numeric capture normalised to {id}
        self.assertIn(("/api/v1/zones/{id}", "GET"), as_set)
        self.assertIn(("/api/v1/zones/{id}/effect", "POST"), as_set)
        # Regex-derived, free-form capture normalised to {name}
        self.assertIn(("/api/v1/presets/{name}", "PUT"), as_set)

        # Comment line must not appear as a registration.
        for route in routes:
            self.assertNotIn("should-be-ignored", route.path)


class FirmwareOnlyDriftTest(unittest.TestCase):
    """Case 2 — drift surfaces firmware-only routes."""

    def test_detects_firmware_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cpp = _write_tmp("V1ApiRoutes.cpp", SAMPLE_CPP, tmp)
            yml = _write_tmp("contract.yaml", SAMPLE_YAML_DRIFTED, tmp)

            firmware = regen_rest.parse_cpp_routes(cpp)
            contract = regen_rest.parse_contract_routes(yml)
            report = regen_rest.compute_drift(firmware, contract)

        firmware_only = {(r.path, r.method) for r in report.firmware_only}
        self.assertIn(("/api/v1/effects/set", "POST"), firmware_only)
        # And nothing else should be flagged firmware-only on this fixture.
        self.assertEqual(len(firmware_only), 1, msg=f"unexpected: {firmware_only}")


class ContractOnlyDriftTest(unittest.TestCase):
    """Case 3 — drift surfaces contract-only routes."""

    def test_detects_contract_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cpp = _write_tmp("V1ApiRoutes.cpp", SAMPLE_CPP, tmp)
            yml = _write_tmp("contract.yaml", SAMPLE_YAML_DRIFTED, tmp)

            firmware = regen_rest.parse_cpp_routes(cpp)
            contract = regen_rest.parse_contract_routes(yml)
            report = regen_rest.compute_drift(firmware, contract)

        contract_only = {(r.path, r.method) for r in report.contract_only}
        self.assertIn(("/api/v1/legacy/ghost-endpoint", "GET"), contract_only)
        self.assertEqual(len(contract_only), 1, msg=f"unexpected: {contract_only}")

    def test_no_drift_on_matched_fixture(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cpp = _write_tmp("V1ApiRoutes.cpp", SAMPLE_CPP, tmp)
            yml = _write_tmp("contract.yaml", SAMPLE_YAML_MATCHED, tmp)

            firmware = regen_rest.parse_cpp_routes(cpp)
            contract = regen_rest.parse_contract_routes(yml)
            report = regen_rest.compute_drift(firmware, contract)

        self.assertEqual(report.firmware_only, [])
        self.assertEqual(report.contract_only, [])


class StrictModeExitTest(unittest.TestCase):
    """Case 4 — strict mode exits non-zero when drift is present."""

    def test_strict_exits_nonzero_on_drift(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cpp = _write_tmp("V1ApiRoutes.cpp", SAMPLE_CPP, tmp)
            yml = _write_tmp("contract.yaml", SAMPLE_YAML_DRIFTED, tmp)
            argv = ["regen_rest.py", "--strict", "--cpp", cpp, "--yaml", yml]
            buf = io.StringIO()
            rc = regen_rest.main(argv, stdout=buf)
        self.assertNotEqual(rc, 0, msg=f"output:\n{buf.getvalue()}")

    def test_strict_exits_zero_when_clean(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cpp = _write_tmp("V1ApiRoutes.cpp", SAMPLE_CPP, tmp)
            yml = _write_tmp("contract.yaml", SAMPLE_YAML_MATCHED, tmp)
            argv = ["regen_rest.py", "--strict", "--cpp", cpp, "--yaml", yml]
            buf = io.StringIO()
            rc = regen_rest.main(argv, stdout=buf)
        self.assertEqual(rc, 0, msg=f"output:\n{buf.getvalue()}")

    def test_default_mode_exits_zero_even_on_drift(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cpp = _write_tmp("V1ApiRoutes.cpp", SAMPLE_CPP, tmp)
            yml = _write_tmp("contract.yaml", SAMPLE_YAML_DRIFTED, tmp)
            argv = ["regen_rest.py", "--cpp", cpp, "--yaml", yml]
            buf = io.StringIO()
            rc = regen_rest.main(argv, stdout=buf)
        self.assertEqual(rc, 0, msg=f"output:\n{buf.getvalue()}")


class UpdateSkeletonTest(unittest.TestCase):
    """Case 5 — --update-skeleton produces a valid YAML file beside the contract."""

    def test_skeleton_written_and_parses(self) -> None:
        import yaml  # local import to keep module-level deps obvious

        with tempfile.TemporaryDirectory() as tmp:
            cpp = _write_tmp("V1ApiRoutes.cpp", SAMPLE_CPP, tmp)
            yml = _write_tmp("contract.yaml", SAMPLE_YAML_DRIFTED, tmp)
            argv = [
                "regen_rest.py",
                "--update-skeleton",
                "--cpp", cpp,
                "--yaml", yml,
            ]
            buf = io.StringIO()
            rc = regen_rest.main(argv, stdout=buf)

            skeleton_path = yml + ".regen.yaml"
            self.assertTrue(os.path.exists(skeleton_path),
                            msg=f"skeleton not written. stdout:\n{buf.getvalue()}")
            self.assertEqual(rc, 0)

            # Existing contract MUST NOT be clobbered.
            with open(yml, "r", encoding="utf-8") as fh:
                self.assertIn("/api/v1/legacy/ghost-endpoint", fh.read())

            with open(skeleton_path, "r", encoding="utf-8") as fh:
                data = yaml.safe_load(fh)

            # Skeleton must be parseable as YAML.
            self.assertIsInstance(data, dict)
            endpoints = data.get("endpoints", {})
            # Firmware-only route from the fixture should appear with FIXME description.
            self.assertIn("/api/v1/effects/set", endpoints)
            entry = endpoints["/api/v1/effects/set"]
            self.assertEqual(entry["method"], "POST")
            self.assertTrue(entry["description"].startswith("FIXME"))
            self.assertEqual(entry["consumers"], [])


if __name__ == "__main__":
    unittest.main()
