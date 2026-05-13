"""
E5 harness self-equivalence proof — zone.setSpeed via SerialJSON.

Phase 0 closeout step 5: prove the harness's send → snapshot → compare loop works
end-to-end against real hardware via at least one already-supported command.

Cross-transport (REST + WS) variants land in Phase 1 once those adapters are wired
and the harness has exercised this loop on hardware.

Methodology:
    1. Snapshot initial zone state via SerialJSON `zones.list`
    2. Send `zone.setSpeed zoneId=1 speed=<X>` via SerialJSON
       (X chosen to be different from current speed)
    3. Snapshot again
    4. Assert zone[1].speed equals X (mutation visible)
    5. Restore original speed (cleanup)
    6. Snapshot, assert restored

This proves the harness can:
    - send commands
    - snapshot state
    - detect mutations
    - distinguish before/after states

All harness building blocks under load. When REST + WS adapters wire in Phase 1, the
test driver swaps transport instances and the comparison logic remains unchanged.
"""

from typing import Tuple


def run(transports: dict) -> Tuple[bool, str]:
    """Entry point invoked by run_all.py. Returns (passed, message)."""
    serial_t = transports.get("serial")
    if serial_t is None:
        return False, "no SerialJsonTransport configured (need --serial-port)"

    # Step 1: initial snapshot
    initial = serial_t.snapshot_zones()
    if initial.zoneCount < 2:
        return False, f"need at least 2 zones for this test, found {initial.zoneCount}"
    # Target Zone 2 — exists in any 2-zone or 3-zone config.
    # Wire format is 0-indexed today (firmware legacy); 1-indexed migration pending per
    # Captain doctrine 2026-05-02. Until migration: send zoneId=1 for user-facing Zone 2.
    target_wire_id = 1  # zoneId in JSON wire format → user-facing Zone 2
    initial_zone = next((z for z in initial.zones if z.id == target_wire_id), None)
    if initial_zone is None:
        return False, f"Zone 2 (wire id {target_wire_id}) not found in initial snapshot"
    original_speed = initial_zone.speed

    # Choose new speed distinct from current
    new_speed = 50 if original_speed != 50 else 75
    if new_speed < 1 or new_speed > 100:
        return False, f"new_speed {new_speed} out of valid range"

    # Step 2: mutate
    resp = serial_t.send({
        "type": "zone.setSpeed",
        "zoneId": target_wire_id,
        "speed": new_speed,
    })
    if not resp.get("success"):
        return False, f"zone.setSpeed returned error: {resp.get('error')}"

    # Step 3: snapshot post-mutation
    post = serial_t.snapshot_zones()
    post_zone = next((z for z in post.zones if z.id == target_wire_id), None)
    if post_zone is None:
        return False, f"zone id {target_wire_id} disappeared from post snapshot"

    # Step 4: assert mutation visible
    if post_zone.speed != new_speed:
        return False, (
            f"mutation NOT visible: zone[{target_wire_id}].speed expected {new_speed}, "
            f"got {post_zone.speed} (initial was {original_speed})"
        )

    # Step 5: restore (cleanup)
    serial_t.send({
        "type": "zone.setSpeed",
        "zoneId": target_wire_id,
        "speed": original_speed,
    })

    # Step 6: snapshot, assert restored
    restored = serial_t.snapshot_zones()
    restored_zone = next((z for z in restored.zones if z.id == target_wire_id), None)
    if restored_zone is None or restored_zone.speed != original_speed:
        return False, (
            f"restore failed: expected speed={original_speed}, got "
            f"{restored_zone.speed if restored_zone else 'None'}"
        )

    return True, (
        f"harness loop verified — zone[{target_wire_id}].speed cycled "
        f"{original_speed} → {new_speed} → {original_speed} via SerialJSON, "
        f"all snapshots reflected the changes correctly"
    )


# Allow running standalone for ad-hoc proof.
if __name__ == "__main__":
    import argparse
    import sys
    sys.path.insert(0, "..")
    from lib.transports import SerialJsonTransport

    parser = argparse.ArgumentParser()
    parser.add_argument("--serial-port", required=True)
    args = parser.parse_args()

    transport = SerialJsonTransport(args.serial_port)
    try:
        ok, msg = run({"serial": transport})
    finally:
        transport.close()
    print(f"{'PASS' if ok else 'FAIL'}: {msg}")
    sys.exit(0 if ok else 1)
