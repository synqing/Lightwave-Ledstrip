#!/usr/bin/env python3
"""
SpectraSynq K1 — 2AFC Validation Stimulus Generator

Drives the ESP32 ValidationMode serial interface to run perceptual validation
studies. Each trial presents two stimuli (LOW vs HIGH on a motion dimension)
and the participant selects which is "more" of the dimension quality.

Protocol: 6 dimensions x 3 reps + 6 catch trials = 24 trials per session.
Counterbalancing: HIGH-first in ~50% of trials, keyed to participant ID.

Usage:
    python stimulus_generator.py [participant_id] [serial_port]
    python stimulus_generator.py 1
    python stimulus_generator.py 1 /dev/cu.usbmodem21401
"""

import serial
import time
import random
import csv
import sys
from dataclasses import dataclass
from typing import List, Optional

SERIAL_PORT = "/dev/cu.usbmodem21401"  # K1v2 default
BAUD_RATE = 115200

# ============================================================================
# Stimulus Definitions — 6 motion dimensions, LOW and HIGH poles
# ============================================================================

STIMULI = {
    "weight": {
        "low": "VAL:PULSE attack=200 decay=2000 intensity=60 origin=all",
        "high": "VAL:PULSE attack=10 decay=300 intensity=255 origin=all",
        "question": "Which animation felt MORE FORCEFUL?",
    },
    "time": {
        "low": "VAL:CHASE speed=20 width=5 trail=200 count=1",
        "high": "VAL:CHASE speed=200 width=3 trail=100 count=3",
        "question": "Which animation felt FASTER / MORE URGENT?",
    },
    "space": {
        "low": "VAL:CHASE speed=80 width=3 trail=200 count=1",
        "high": "VAL:NOISE speed=0.5 scale=0.3 palette=0",
        "question": "Which animation felt MORE FOCUSED / DIRECTED?",
    },
    "flow": {
        "low": "VAL:WAVE waveform=square freq=2.0 speed=1.0 amp=200",
        "high": "VAL:WAVE waveform=sine freq=0.5 speed=0.3 amp=180",
        "question": "Which animation felt MORE FREE / FLOWING?",
    },
    "fluidity": {
        "low": "VAL:SPARKLE density=80 fade=100",
        "high": "VAL:WAVE waveform=sine freq=1.0 speed=0.5 amp=200",
        "question": "Which animation felt SMOOTHER?",
    },
    "impulse": {
        "low": "VAL:FADE start_h=0 start_s=0 start_v=0 end_h=0 end_s=0 end_v=200 duration=500",
        "high": "VAL:PULSE attack=5 decay=500 intensity=255 origin=center",
        "question": "Which animation had a SHARPER / MORE EXPLOSIVE start?",
    },
}

STIMULUS_DURATION_SEC = 5.0
ISI_SEC = 1.0  # inter-stimulus interval
REPS_PER_DIM = 3
CATCH_TRIALS = 6


# ============================================================================
# Data Classes
# ============================================================================

@dataclass
class Trial:
    dimension: str
    correct_answer: str  # "A" or "B" (or "CATCH")
    is_catch: bool
    stimulus_a: str  # serial command
    stimulus_b: str


@dataclass
class TrialResult:
    trial_num: int
    dimension: str
    correct_answer: str
    participant_answer: str
    correct: bool
    rt_ms: float
    is_catch: bool


# ============================================================================
# Trial Generation
# ============================================================================

def generate_trial_list(participant_id: int) -> List[Trial]:
    """Generate randomized trial list with counterbalancing."""
    trials = []
    for dim in STIMULI:
        for rep in range(REPS_PER_DIM):
            # Counterbalance: HIGH first in ~50% of trials
            high_first = (rep + participant_id) % 2 == 0
            if high_first:
                trials.append(Trial(dim, "A", False,
                                    STIMULI[dim]["high"], STIMULI[dim]["low"]))
            else:
                trials.append(Trial(dim, "B", False,
                                    STIMULI[dim]["low"], STIMULI[dim]["high"]))

    # Add catch trials (identical stimuli — no correct answer)
    catch_dims = random.sample(list(STIMULI.keys()),
                               min(CATCH_TRIALS, len(STIMULI)))
    for dim in catch_dims:
        level = random.choice(["low", "high"])
        cmd = STIMULI[dim][level]
        trials.append(Trial(dim, "CATCH", True, cmd, cmd))

    random.shuffle(trials)
    return trials


# ============================================================================
# Trial Execution
# ============================================================================

def send_command(ser: serial.Serial, cmd: str) -> None:
    """Send a command string to the device."""
    ser.write(f"{cmd}\n".encode())


def run_trial(ser: serial.Serial, trial: Trial, trial_num: int) -> TrialResult:
    """Execute a single 2AFC trial."""
    # Present stimulus A
    send_command(ser, trial.stimulus_a)
    time.sleep(STIMULUS_DURATION_SEC)

    # Inter-stimulus interval
    send_command(ser, "VAL:CLEAR")
    time.sleep(ISI_SEC)

    # Present stimulus B
    send_command(ser, trial.stimulus_b)
    time.sleep(STIMULUS_DURATION_SEC)

    send_command(ser, "VAL:CLEAR")

    # Get response
    print(f"\n  {STIMULI[trial.dimension]['question']}")
    print("  Press [A] for first, [B] for second: ", end="", flush=True)

    t0 = time.time()
    while True:
        resp = input().strip().upper()
        if resp in ("A", "B"):
            break
        print("  Please press A or B: ", end="", flush=True)
    rt_ms = (time.time() - t0) * 1000

    correct = (resp == trial.correct_answer) if not trial.is_catch else True

    return TrialResult(trial_num, trial.dimension, trial.correct_answer,
                       resp, correct, rt_ms, trial.is_catch)


# ============================================================================
# Session Runner
# ============================================================================

def run_session(participant_id: int, port: str = SERIAL_PORT) -> None:
    """Run complete validation session for one participant."""
    ser = serial.Serial(port, BAUD_RATE, timeout=1)
    time.sleep(2)  # Wait for ESP32 reset after serial open

    # Handshake
    send_command(ser, "VAL:ACK")
    resp = ser.readline().decode().strip()
    if "READY" not in resp:
        print(f"ERROR: Device not ready (got: '{resp}')")
        ser.close()
        return

    print(f"=== Validation Study -- Participant {participant_id} ===")
    print(f"    Port: {port}")
    print(f"    Stimulus duration: {STIMULUS_DURATION_SEC}s")
    print(f"    ISI: {ISI_SEC}s")

    trials = generate_trial_list(participant_id)
    results: List[TrialResult] = []

    # -- Practice block (3 trials with feedback) --
    print("\n--- Practice Block (3 trials with feedback) ---")
    practice_dims = ["weight", "time", "impulse"]
    for i, dim in enumerate(practice_dims):
        print(f"\nPractice {i+1}/3:")
        t = Trial(dim, "B", False,
                  STIMULI[dim]["low"], STIMULI[dim]["high"])
        r = run_trial(ser, t, -(i + 1))
        if r.correct:
            print(f"  CORRECT -- the HIGH {dim} was stimulus B")
        else:
            print(f"  INCORRECT -- the HIGH {dim} was stimulus B")

    # -- Main block --
    print(f"\n--- Main Block ({len(trials)} trials) ---")
    for i, trial in enumerate(trials):
        print(f"\nTrial {i+1}/{len(trials)}:")
        result = run_trial(ser, trial, i + 1)
        results.append(result)
        tag = "correct" if result.correct else "wrong"
        if result.is_catch:
            tag = "catch"
        print(f"  Response: {result.participant_answer} ({tag}) "
              f"RT={result.rt_ms:.0f}ms")

    # -- Save results --
    filename = f"validation_p{participant_id:03d}.csv"
    with open(filename, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["trial", "dimension", "correct_answer",
                         "response", "correct", "rt_ms", "is_catch"])
        for r in results:
            writer.writerow([r.trial_num, r.dimension, r.correct_answer,
                             r.participant_answer, r.correct,
                             f"{r.rt_ms:.1f}", r.is_catch])

    # -- Summary --
    print(f"\n=== Session Complete ===")
    print(f"Results saved to: {filename}")
    for dim in STIMULI:
        dim_results = [r for r in results
                       if r.dimension == dim and not r.is_catch]
        if dim_results:
            acc = sum(r.correct for r in dim_results) / len(dim_results)
            n_correct = sum(r.correct for r in dim_results)
            print(f"  {dim:12s}: {acc*100:.0f}% "
                  f"({n_correct}/{len(dim_results)})")

    catch_results = [r for r in results if r.is_catch]
    if catch_results:
        print(f"  {'catch':12s}: {len(catch_results)} trials")

    ser.close()


# ============================================================================
# Entry Point
# ============================================================================

if __name__ == "__main__":
    pid = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    port = sys.argv[2] if len(sys.argv) > 2 else SERIAL_PORT
    run_session(pid, port)
