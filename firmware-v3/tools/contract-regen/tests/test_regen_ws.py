"""Tests for regen_ws.py — WebSocket contract drift detector.

Run from repo root:
    python3 -m unittest discover firmware-v3/tools/contract-regen/tests
"""
import os
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

# Make the parent package importable when tests are discovered from repo root.
TOOL_DIR = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(TOOL_DIR))

import regen_ws  # noqa: E402  (sys.path manipulation precedes import)


class ParseSourceFileTests(unittest.TestCase):
    """Case 1: parses a sample WsXxxCommands.cpp snippet correctly."""

    def test_parses_simple_register_calls(self) -> None:
        source = textwrap.dedent(
            """
            #include "WsCommandRouter.h"

            void registerAuthCommands() {
                WsCommandRouter::registerCommand("auth.status", handleAuthStatus);
                WsCommandRouter::registerCommand("auth.rotate", handleAuthRotate);
            }
            """
        )
        commands = regen_ws.parse_registered_commands(source)
        self.assertEqual(commands, ["auth.status", "auth.rotate"])

    def test_handles_dotted_and_legacy_names(self) -> None:
        source = textwrap.dedent(
            """
            WsCommandRouter::registerCommand("setEffect", handleSetEffect);
            WsCommandRouter::registerCommand("effects.setCurrent", handleEffectsSetCurrent);
            """
        )
        commands = regen_ws.parse_registered_commands(source)
        self.assertIn("setEffect", commands)
        self.assertIn("effects.setCurrent", commands)


class CommentHandlingTests(unittest.TestCase):
    """Case 5: skips commented-out registrations (// and /* */)."""

    def test_skips_line_comments(self) -> None:
        source = textwrap.dedent(
            """
            WsCommandRouter::registerCommand("real.cmd", handleReal);
            // WsCommandRouter::registerCommand("commented.cmd", handleCommented);
                // WsCommandRouter::registerCommand("indented.commented", handler);
            """
        )
        commands = regen_ws.parse_registered_commands(source)
        self.assertEqual(commands, ["real.cmd"])

    def test_skips_block_comments(self) -> None:
        source = textwrap.dedent(
            """
            WsCommandRouter::registerCommand("alive.one", handlerOne);
            /*
            WsCommandRouter::registerCommand("dead.one", handlerDead1);
            WsCommandRouter::registerCommand("dead.two", handlerDead2);
            */
            WsCommandRouter::registerCommand("alive.two", handlerTwo);
            """
        )
        commands = regen_ws.parse_registered_commands(source)
        self.assertEqual(commands, ["alive.one", "alive.two"])

    def test_skips_multiline_block_comment_with_content_after_close(self) -> None:
        source = textwrap.dedent(
            """
            /* WsCommandRouter::registerCommand("dead", handlerDead); */
            WsCommandRouter::registerCommand("alive", handlerAlive);
            """
        )
        commands = regen_ws.parse_registered_commands(source)
        self.assertEqual(commands, ["alive"])


class AggregationTests(unittest.TestCase):
    """Case 2: aggregates across multiple .cpp files via glob."""

    def test_collect_from_directory(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            (tmp_path / "WsAlphaCommands.cpp").write_text(
                'WsCommandRouter::registerCommand("alpha.one", handleAlphaOne);\n'
                'WsCommandRouter::registerCommand("alpha.two", handleAlphaTwo);\n'
            )
            (tmp_path / "WsBetaCommands.cpp").write_text(
                'WsCommandRouter::registerCommand("beta.one", handleBetaOne);\n'
            )
            # Non-matching file should be ignored.
            (tmp_path / "WsBetaCommands.h").write_text(
                'WsCommandRouter::registerCommand("ignored", handler);\n'
            )

            commands = regen_ws.collect_firmware_commands(tmp_path)
            self.assertEqual(
                sorted(commands.keys()),
                ["alpha.one", "alpha.two", "beta.one"],
            )
            # Each command should map back to the file it came from.
            self.assertTrue(commands["alpha.one"].name == "WsAlphaCommands.cpp")
            self.assertTrue(commands["beta.one"].name == "WsBetaCommands.cpp")


class DriftDetectionTests(unittest.TestCase):
    """Cases 3 + 4: detects firmware-only and contract-only commands."""

    def test_firmware_only_detection(self) -> None:
        firmware = {"a", "b", "c"}
        contract = {"a", "b"}
        report = regen_ws.compute_drift(firmware, contract)
        self.assertEqual(report.firmware_only, ["c"])
        self.assertEqual(report.contract_only, [])

    def test_contract_only_detection(self) -> None:
        firmware = {"a"}
        contract = {"a", "obsolete"}
        report = regen_ws.compute_drift(firmware, contract)
        self.assertEqual(report.firmware_only, [])
        self.assertEqual(report.contract_only, ["obsolete"])

    def test_both_directions(self) -> None:
        firmware = {"new.cmd", "shared"}
        contract = {"shared", "ghost.cmd"}
        report = regen_ws.compute_drift(firmware, contract)
        self.assertEqual(report.firmware_only, ["new.cmd"])
        self.assertEqual(report.contract_only, ["ghost.cmd"])
        self.assertEqual(report.total_drift, 2)


class StrictExitTests(unittest.TestCase):
    """Case 6: --strict exits non-zero on drift, zero on no drift."""

    def setUp(self) -> None:
        self.tmpdir = tempfile.TemporaryDirectory()
        self.tmp = Path(self.tmpdir.name)
        self.ws_dir = self.tmp / "ws"
        self.ws_dir.mkdir()
        self.contract_path = self.tmp / "contract.yaml"

    def tearDown(self) -> None:
        self.tmpdir.cleanup()

    def _write_firmware(self, names: list[str]) -> None:
        body = "\n".join(
            f'WsCommandRouter::registerCommand("{n}", h{i});'
            for i, n in enumerate(names)
        )
        (self.ws_dir / "WsSampleCommands.cpp").write_text(body + "\n")

    def _write_contract(self, names: list[str]) -> None:
        lines = ["commands:"]
        for n in names:
            lines.append(f'  "{n}":')
            lines.append('    direction: "client -> K1"')
        self.contract_path.write_text("\n".join(lines) + "\n")

    def test_strict_passes_when_no_drift(self) -> None:
        self._write_firmware(["a", "b"])
        self._write_contract(["a", "b"])
        rc = regen_ws.main(
            [
                "--ws-dir",
                str(self.ws_dir),
                "--contract",
                str(self.contract_path),
                "--strict",
            ]
        )
        self.assertEqual(rc, 0)

    def test_strict_fails_on_firmware_only_drift(self) -> None:
        self._write_firmware(["a", "b", "extra"])
        self._write_contract(["a", "b"])
        rc = regen_ws.main(
            [
                "--ws-dir",
                str(self.ws_dir),
                "--contract",
                str(self.contract_path),
                "--strict",
            ]
        )
        self.assertNotEqual(rc, 0)

    def test_strict_fails_on_contract_only_drift(self) -> None:
        self._write_firmware(["a"])
        self._write_contract(["a", "ghost"])
        rc = regen_ws.main(
            [
                "--ws-dir",
                str(self.ws_dir),
                "--contract",
                str(self.contract_path),
                "--strict",
            ]
        )
        self.assertNotEqual(rc, 0)


class UpdateSkeletonTests(unittest.TestCase):
    """--update-skeleton writes *.regen.yaml with placeholder entries."""

    def test_skeleton_contains_firmware_only_commands(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            ws_dir = tmp_path / "ws"
            ws_dir.mkdir()
            (ws_dir / "WsSampleCommands.cpp").write_text(
                'WsCommandRouter::registerCommand("kept", h1);\n'
                'WsCommandRouter::registerCommand("missing.from.contract", h2);\n'
            )
            contract_path = tmp_path / "contract.yaml"
            contract_path.write_text(
                "commands:\n"
                '  "kept":\n'
                '    direction: "client -> K1"\n'
            )
            rc = regen_ws.main(
                [
                    "--ws-dir",
                    str(ws_dir),
                    "--contract",
                    str(contract_path),
                    "--update-skeleton",
                ]
            )
            self.assertEqual(rc, 0)
            skeleton_path = contract_path.with_suffix(".regen.yaml")
            self.assertTrue(skeleton_path.exists())
            text = skeleton_path.read_text()
            self.assertIn("missing.from.contract", text)
            # Placeholder marker should be present so the human knows to fill in.
            self.assertIn("TODO", text)


if __name__ == "__main__":
    unittest.main()
