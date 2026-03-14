"""
EdgeMixer evaluation CLI — main entry point.

Subcommands
-----------
single     Evaluate a single EdgeMixer profile on one K1.
shootout   Compare multiple profiles on one K1.
dual       Control vs treatment on two K1 devices.
list       List available profiles.

Usage examples::

    python -m edgemixer_eval single --port /dev/cu.usbmodem21401 --profile split_comp
    python -m edgemixer_eval shootout --port /dev/cu.usbmodem21401 --frames 150
    python -m edgemixer_eval dual --port-control /dev/cu.usbmodem21401 \\
        --port-treatment /dev/cu.usbmodem212401 --profile-treatment analogous_gradient
    python -m edgemixer_eval list
"""

from __future__ import annotations

import argparse
import logging
import os
import sys
import time
from datetime import datetime, timezone

from .device import K1Device
from .metrics import compute_delta, compute_run_metrics
from .profiles import (
    PROFILES,
    SHOOTOUT_DEFAULT,
    STIMULUS_CATEGORIES,
    get_profile,
    list_profiles,
)
from .report import (
    check_safety_gates,
    export_json,
    format_comparison,
    format_run_summary,
    format_shootout,
)

logger = logging.getLogger(__name__)

# Default capture parameters.
DEFAULT_FRAMES = 300   # ~10 seconds at 30 FPS
DEFAULT_FPS = 30
SHOOTOUT_FRAMES = 150  # ~5 seconds per profile in shootout mode


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _resolve_output_path(output_dir: str | None, label: str) -> str | None:
    """Build a JSON output path, or return None if no output dir specified."""
    if output_dir is None:
        return None
    os.makedirs(output_dir, exist_ok=True)
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    return os.path.join(output_dir, f"edgemixer_{label}_{ts}.json")


def _build_run_metadata(
    port: str,
    profile_name: str,
    n_frames: int,
    fps: int,
    stimulus: str | None,
) -> dict:
    """Build metadata dict for a run."""
    return {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "port": port,
        "profile": profile_name,
        "n_frames": n_frames,
        "fps": fps,
        "stimulus": stimulus,
    }


def _capture_and_compute(
    device: K1Device,
    profile_name: str,
    n_frames: int,
    fps: int,
) -> dict:
    """Capture frames from a device and compute run metrics.

    Returns the run_metrics dict from ``compute_run_metrics``.
    """
    profile = get_profile(profile_name)
    if profile is None:
        raise ValueError(f"Unknown profile: {profile_name!r}")

    device.set_edgemixer(profile, settle_time=0.5)

    raw_frames = device.capture_frames(n_frames=n_frames, fps=fps, tap="c")

    if not raw_frames:
        logger.error("No frames captured from device.")
        return {"n_frames": 0}

    # Convert (frame, FrameMetadata) tuples to (frame, dict) for metrics.
    frames_and_meta = []
    for frame, meta in raw_frames:
        meta_dict = {
            "rms": meta.rms if meta.rms is not None else float("nan"),
            "beat": meta.beat if meta.beat is not None else False,
        }
        frames_and_meta.append((frame, meta_dict))

    return compute_run_metrics(frames_and_meta)


# ---------------------------------------------------------------------------
# Subcommand: single
# ---------------------------------------------------------------------------

def cmd_single(args: argparse.Namespace) -> int:
    """Evaluate a single EdgeMixer profile."""
    profile = get_profile(args.profile)
    if profile is None:
        print(f"Error: Unknown profile '{args.profile}'.", file=sys.stderr)
        print(f"Available: {', '.join(list_profiles())}", file=sys.stderr)
        return 1

    print(f"Connecting to K1 on {args.port}...")
    try:
        device = K1Device(args.port)
    except Exception as e:
        print(f"Error: Could not open serial port {args.port}: {e}", file=sys.stderr)
        return 1

    try:
        if hasattr(args, "effect") and args.effect is not None:
            effect_id = int(args.effect, 0)
            print(f"Setting effect 0x{effect_id:04X} at brightness 255...")
            device.set_effect(effect_id, brightness=255)

        print(f"Evaluating profile: {profile.name} ({profile.description})")
        print(f"Capturing {args.frames} frames at {args.fps} FPS...")

        run_metrics = _capture_and_compute(device, args.profile, args.frames, args.fps)

        if run_metrics.get("n_frames", 0) == 0:
            print("Error: No frames captured. Check device connection.", file=sys.stderr)
            return 1

        safety = check_safety_gates(run_metrics, profile.name)

        # Print summary.
        print()
        print(format_run_summary(run_metrics, profile, safety))

        # Export JSON if requested.
        if args.output:
            out_path = _resolve_output_path(args.output, f"single_{profile.name}")
            if out_path:
                results = [{
                    "profile": profile,
                    "run_metrics": run_metrics,
                    "safety": safety,
                    "metadata": _build_run_metadata(
                        args.port, args.profile, args.frames, args.fps, args.stimulus,
                    ),
                }]
                export_json(results, out_path)
                print(f"\nJSON exported to: {out_path}")

    finally:
        device.close()

    return 0


# ---------------------------------------------------------------------------
# Subcommand: shootout
# ---------------------------------------------------------------------------

def cmd_shootout(args: argparse.Namespace) -> int:
    """Compare multiple EdgeMixer profiles."""
    # Determine profile list.
    if args.profiles:
        profile_names = [p.strip() for p in args.profiles.split(",")]
    else:
        profile_names = list(SHOOTOUT_DEFAULT)

    # Ensure MIRROR is included as baseline.
    if "mirror" not in profile_names:
        profile_names.insert(0, "mirror")

    # Validate all profile names.
    for pname in profile_names:
        if get_profile(pname) is None:
            print(f"Error: Unknown profile '{pname}'.", file=sys.stderr)
            print(f"Available: {', '.join(list_profiles())}", file=sys.stderr)
            return 1

    print(f"Connecting to K1 on {args.port}...")
    try:
        device = K1Device(args.port)
    except Exception as e:
        print(f"Error: Could not open serial port {args.port}: {e}", file=sys.stderr)
        return 1

    results: list[dict] = []

    try:
        # Set the test effect if specified (port open resets the device).
        if hasattr(args, "effect") and args.effect is not None:
            effect_id = int(args.effect, 0)  # Supports hex like 0x0100
            print(f"Setting effect 0x{effect_id:04X} at brightness 255...")
            device.set_effect(effect_id, brightness=255)

        total = len(profile_names)
        for idx, pname in enumerate(profile_names, 1):
            profile = get_profile(pname)
            assert profile is not None  # Validated above.

            print(f"\n[{idx}/{total}] Evaluating: {pname} ({profile.description})")
            print(f"  Capturing {args.frames} frames at {args.fps} FPS...")

            run_metrics = _capture_and_compute(device, pname, args.frames, args.fps)

            if run_metrics.get("n_frames", 0) == 0:
                print(f"  Warning: No frames captured for {pname}.", file=sys.stderr)

            safety = check_safety_gates(run_metrics, pname)
            status = "PASS" if safety["overall_pass"] else "FAIL"
            n = run_metrics.get("n_frames", 0)
            print(f"  Done. {n} frames, safety={status}")

            results.append({
                "profile": profile,
                "run_metrics": run_metrics,
                "safety": safety,
                "metadata": _build_run_metadata(
                    args.port, pname, args.frames, args.fps, args.stimulus,
                ),
            })

    finally:
        device.close()

    # Print shootout table.
    print()
    print(format_shootout(results))

    # Print per-profile comparison vs MIRROR.
    mirror_entry = next((r for r in results if r["profile"].name == "mirror"), None)
    if mirror_entry:
        for entry in results:
            if entry["profile"].name == "mirror":
                continue
            delta = compute_delta(mirror_entry["run_metrics"], entry["run_metrics"])
            print()
            print(format_comparison(
                mirror_entry["run_metrics"],
                entry["run_metrics"],
                delta,
                mirror_entry["profile"],
                entry["profile"],
                mirror_entry["safety"],
                entry["safety"],
            ))

    # Export JSON if requested.
    if args.output:
        out_path = _resolve_output_path(args.output, "shootout")
        if out_path:
            export_json(results, out_path)
            print(f"\nJSON exported to: {out_path}")

    return 0


# ---------------------------------------------------------------------------
# Subcommand: dual
# ---------------------------------------------------------------------------

def cmd_dual(args: argparse.Namespace) -> int:
    """Control vs treatment evaluation on two K1 devices."""
    treatment_profile = get_profile(args.profile_treatment)
    if treatment_profile is None:
        print(f"Error: Unknown profile '{args.profile_treatment}'.", file=sys.stderr)
        print(f"Available: {', '.join(list_profiles())}", file=sys.stderr)
        return 1

    mirror_profile = get_profile("mirror")
    assert mirror_profile is not None

    # Connect to both devices.
    print(f"Connecting to control K1 on {args.port_control}...")
    try:
        dev_control = K1Device(args.port_control)
    except Exception as e:
        print(f"Error: Could not open control port {args.port_control}: {e}", file=sys.stderr)
        return 1

    print(f"Connecting to treatment K1 on {args.port_treatment}...")
    try:
        dev_treatment = K1Device(args.port_treatment)
    except Exception as e:
        print(f"Error: Could not open treatment port {args.port_treatment}: {e}", file=sys.stderr)
        dev_control.close()
        return 1

    try:
        # Set test effect on both devices if specified.
        if hasattr(args, "effect") and args.effect is not None:
            effect_id = int(args.effect, 0)
            print(f"Setting effect 0x{effect_id:04X} on both devices...")
            dev_control.set_effect(effect_id, brightness=255)
            dev_treatment.set_effect(effect_id, brightness=255)

        # Configure devices.
        print(f"Setting control K1 to MIRROR...")
        dev_control.set_edgemixer(mirror_profile, settle_time=0.5)

        print(f"Setting treatment K1 to {treatment_profile.name}...")
        dev_treatment.set_edgemixer(treatment_profile, settle_time=0.5)

        # Capture from control device.
        print(f"\nCapturing {args.frames} frames from control (MIRROR) at {args.fps} FPS...")
        control_metrics = _capture_and_compute(
            dev_control, "mirror", args.frames, args.fps,
        )

        # Capture from treatment device.
        print(f"Capturing {args.frames} frames from treatment ({treatment_profile.name}) at {args.fps} FPS...")
        treatment_metrics = _capture_and_compute(
            dev_treatment, args.profile_treatment, args.frames, args.fps,
        )

        if control_metrics.get("n_frames", 0) == 0:
            print("Error: No frames captured from control device.", file=sys.stderr)
            return 1
        if treatment_metrics.get("n_frames", 0) == 0:
            print("Error: No frames captured from treatment device.", file=sys.stderr)
            return 1

        # Compute delta and safety.
        delta = compute_delta(control_metrics, treatment_metrics)
        control_safety = check_safety_gates(control_metrics, "mirror")
        treatment_safety = check_safety_gates(treatment_metrics, treatment_profile.name)

        # Print comparison.
        print()
        print(format_comparison(
            control_metrics,
            treatment_metrics,
            delta,
            mirror_profile,
            treatment_profile,
            control_safety,
            treatment_safety,
        ))

        # Export JSON if requested.
        if args.output:
            out_path = _resolve_output_path(args.output, f"dual_{treatment_profile.name}")
            if out_path:
                results = [
                    {
                        "profile": mirror_profile,
                        "run_metrics": control_metrics,
                        "safety": control_safety,
                        "delta": delta,
                        "metadata": _build_run_metadata(
                            args.port_control, "mirror",
                            args.frames, args.fps, args.stimulus,
                        ),
                    },
                    {
                        "profile": treatment_profile,
                        "run_metrics": treatment_metrics,
                        "safety": treatment_safety,
                        "delta": delta,
                        "metadata": _build_run_metadata(
                            args.port_treatment, args.profile_treatment,
                            args.frames, args.fps, args.stimulus,
                        ),
                    },
                ]
                export_json(results, out_path)
                print(f"\nJSON exported to: {out_path}")

    finally:
        dev_control.close()
        dev_treatment.close()

    return 0


# ---------------------------------------------------------------------------
# Subcommand: list
# ---------------------------------------------------------------------------

def cmd_list(args: argparse.Namespace) -> int:
    """List all available profiles."""
    print("EdgeMixer Evaluation Profiles")
    print("=" * 60)
    print()

    for name in list_profiles():
        profile = get_profile(name)
        assert profile is not None
        shootout_marker = " [shootout]" if name in SHOOTOUT_DEFAULT else ""
        print(f"  {name:24s}  {profile.description}{shootout_marker}")
        print(f"    mode={profile.mode}  spread={profile.spread}  "
              f"strength={profile.strength}  spatial={profile.spatial}  "
              f"temporal={profile.temporal}")
        print()

    print("Stimulus Categories")
    print("-" * 60)
    for cat, desc in STIMULUS_CATEGORIES.items():
        print(f"  {cat:20s}  {desc}")

    return 0


# ---------------------------------------------------------------------------
# Argument parser
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    """Build the CLI argument parser."""
    parser = argparse.ArgumentParser(
        prog="edgemixer_eval",
        description="EdgeMixer evaluation harness for LightwaveOS K1 devices.",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose (DEBUG) logging.",
    )

    subparsers = parser.add_subparsers(dest="command", help="Evaluation mode")

    # -- single --
    sp_single = subparsers.add_parser(
        "single",
        help="Evaluate a single EdgeMixer profile.",
    )
    sp_single.add_argument("--port", required=True, help="Serial port for the K1 device.")
    sp_single.add_argument("--profile", required=True, help="Profile name to evaluate.")
    sp_single.add_argument("--frames", type=int, default=DEFAULT_FRAMES,
                           help=f"Number of frames to capture (default: {DEFAULT_FRAMES}).")
    sp_single.add_argument("--fps", type=int, default=DEFAULT_FPS,
                           help=f"Capture FPS (default: {DEFAULT_FPS}).")
    sp_single.add_argument("--output", default=None,
                           help="Output directory for JSON export.")
    sp_single.add_argument("--stimulus", default=None, choices=list(STIMULUS_CATEGORIES.keys()),
                           help="Stimulus category label for this run.")
    sp_single.add_argument("--effect", default=None,
                           help="Effect ID to set before evaluation (hex, e.g. 0x1100).")

    # -- shootout --
    sp_shootout = subparsers.add_parser(
        "shootout",
        help="Compare multiple EdgeMixer profiles.",
    )
    sp_shootout.add_argument("--port", required=True, help="Serial port for the K1 device.")
    sp_shootout.add_argument("--profiles", default=None,
                             help="Comma-separated profile names (default: shootout set).")
    sp_shootout.add_argument("--frames", type=int, default=SHOOTOUT_FRAMES,
                             help=f"Frames per profile (default: {SHOOTOUT_FRAMES}).")
    sp_shootout.add_argument("--fps", type=int, default=DEFAULT_FPS,
                             help=f"Capture FPS (default: {DEFAULT_FPS}).")
    sp_shootout.add_argument("--output", default=None,
                             help="Output directory for JSON export.")
    sp_shootout.add_argument("--stimulus", default=None, choices=list(STIMULUS_CATEGORIES.keys()),
                             help="Stimulus category label for this run.")
    sp_shootout.add_argument("--effect", default=None,
                             help="Effect ID to set before evaluation (hex, e.g. 0x1100).")

    # -- dual --
    sp_dual = subparsers.add_parser(
        "dual",
        help="Control vs treatment on two K1 devices.",
    )
    sp_dual.add_argument("--port-control", required=True,
                         help="Serial port for the control K1 (set to MIRROR).")
    sp_dual.add_argument("--port-treatment", required=True,
                         help="Serial port for the treatment K1.")
    sp_dual.add_argument("--profile-treatment", required=True,
                         help="Profile name for the treatment K1.")
    sp_dual.add_argument("--frames", type=int, default=DEFAULT_FRAMES,
                         help=f"Frames to capture from each device (default: {DEFAULT_FRAMES}).")
    sp_dual.add_argument("--fps", type=int, default=DEFAULT_FPS,
                         help=f"Capture FPS (default: {DEFAULT_FPS}).")
    sp_dual.add_argument("--output", default=None,
                         help="Output directory for JSON export.")
    sp_dual.add_argument("--stimulus", default=None, choices=list(STIMULUS_CATEGORIES.keys()),
                         help="Stimulus category label for this run.")
    sp_dual.add_argument("--effect", default=None,
                         help="Effect ID to set on both devices (hex, e.g. 0x1100).")

    # -- list --
    subparsers.add_parser(
        "list",
        help="List all available profiles and stimulus categories.",
    )

    return parser


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    """Entry point for the EdgeMixer evaluation CLI.

    Parameters
    ----------
    argv : list of str, optional
        Command-line arguments. Uses ``sys.argv[1:]`` if None.

    Returns
    -------
    int
        Exit code (0 = success, non-zero = error).
    """
    parser = build_parser()
    args = parser.parse_args(argv)

    # Configure logging.
    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )

    if args.command is None:
        parser.print_help()
        return 1

    dispatch = {
        "single": cmd_single,
        "shootout": cmd_shootout,
        "dual": cmd_dual,
        "list": cmd_list,
    }

    handler = dispatch.get(args.command)
    if handler is None:
        parser.print_help()
        return 1

    return handler(args)


if __name__ == "__main__":
    sys.exit(main())
