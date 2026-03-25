# Onset Capture Workflow

Use this when the onset detector needs end-to-end validation on real K1 hardware.

## Purpose

`tools/led_capture/capture_onset_suite.py` reuses the existing serial capture and
analysis stack, but adds onset-specific fixtures and gates:

- transport health still checked via the existing regression gate;
- onset liveliness and saturation checked per fixture;
- fixed fixtures used instead of ad hoc music.

## Default Fixtures

The suite currently runs five fixtures:

| Fixture | Source | Purpose |
|---------|--------|---------|
| `silence` | generated | false-trigger floor |
| `click120` | generated | clean onset timing |
| `four4_128` | generated | steady 4/4 pulse behaviour |
| `sparse_ballad` | committed harmonixset WAV | sparse/dynamic music |
| `dense_music` | committed harmonixset WAV | dense transient music |

The synthetic click fixtures exist because the generic capture docs required
click tracks, but none were committed in this repo.

## K1v2 Command

```bash
python tools/led_capture/capture_onset_suite.py \
  --serial /dev/cu.usbmodem1101 \
  --output /tmp/onset_k1v2
```

On macOS the runner defaults to `afplay {clip}` for fixture playback.

## Outputs

Each fixture produces:

- `<fixture>.npz` raw capture session
- `<fixture>.png` dashboard
- `onset_suite_report.txt` combined pass/fail report

## Important Assumptions

- The board must already be running firmware with v2 capture metadata.
- The capture runner does not inject audio directly into firmware; it plays
  fixtures externally and assumes the real audio path hears them.
- If automatic playback is not appropriate, disable it with `--no-playback`
  and handle playback manually.

## Recommended K1v2 Check

Before trusting a result:

1. Verify the correct K1v2 firmware is already flashed.
2. Keep the board close to the playback source or line-input path.
3. Run the onset suite.
4. If any fixture fails, inspect the saved dashboard and raw session before
   retuning detector thresholds.
