#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2025-2026 SpectraSynq
"""Unit tests for capture_c1_envelope.py host-side planning helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

_TOOLS_DIR = Path(__file__).parent.parent.parent / "tools"
if str(_TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(_TOOLS_DIR))

import capture_c1_envelope as c1


class TestC1LineParser(unittest.TestCase):
    def test_parse_c1_line_extracts_all_metrics(self):
        parsed = c1.parse_c1_line(
            "[C1] t_us=123456 raw=0.001234 frame=0.500000 "
            "conf=0.750 sil=0.250 silent=1 peak=0.333 peakLast=0.222"
        )

        self.assertIsNotNone(parsed)
        assert parsed is not None
        self.assertEqual(parsed["t_us"], 123456)
        self.assertAlmostEqual(parsed["rawHopRms"], 0.001234)
        self.assertAlmostEqual(parsed["frameRms"], 0.5)
        self.assertAlmostEqual(parsed["audioConfidence"], 0.75)
        self.assertAlmostEqual(parsed["silentScale"], 0.25)
        self.assertEqual(parsed["isSilent"], 1)
        self.assertAlmostEqual(parsed["waveformPeakScaled"], 0.333)
        self.assertAlmostEqual(parsed["waveformPeakScaledLast"], 0.222)


class TestPlaybackManifest(unittest.TestCase):
    def test_normalise_manifest_requires_explicit_armed_execution_plan(self):
        manifest = {
            "tracks": [
                {"name": "idle_room", "captureSeconds": 30},
                {
                    "name": "normal_music",
                    "path": "fixtures/normal.wav",
                    "captureSeconds": 45,
                },
                {
                    "name": "stop_recovery",
                    "path": "fixtures/stop.wav",
                    "captureSeconds": 40,
                    "hardStopAfterSeconds": 20,
                },
            ]
        }

        regimes = c1.normalise_playback_manifest(manifest)

        self.assertEqual([r.name for r in regimes], ["idle_room", "normal_music", "stop_recovery"])
        self.assertIsNone(regimes[0].path)
        self.assertEqual(regimes[1].path, Path("fixtures/normal.wav"))
        self.assertEqual(regimes[2].hard_stop_after_seconds, 20.0)

    def test_normalise_manifest_rejects_stop_after_capture_window(self):
        manifest = {
            "tracks": [
                {
                    "name": "stop_recovery",
                    "path": "fixtures/stop.wav",
                    "captureSeconds": 10,
                    "hardStopAfterSeconds": 20,
                },
            ]
        }

        with self.assertRaises(ValueError):
            c1.normalise_playback_manifest(manifest)


class TestC1Summary(unittest.TestCase):
    def test_summarise_c1_samples_reports_post_stop_recovery(self):
        samples = [
            {
                "t_us": 1_000_000,
                "rawHopRms": 0.03,
                "frameRms": 0.8,
                "audioConfidence": 1.0,
                "silentScale": 1.0,
                "isSilent": 0,
                "waveformPeakScaled": 0.4,
                "waveformPeakScaledLast": 0.4,
            },
            {
                "t_us": 21_100_000,
                "rawHopRms": 0.002,
                "frameRms": 0.2,
                "audioConfidence": 0.7,
                "silentScale": 0.5,
                "isSilent": 1,
                "waveformPeakScaled": 0.1,
                "waveformPeakScaledLast": 0.2,
            },
            {
                "t_us": 22_200_000,
                "rawHopRms": 0.001,
                "frameRms": 0.1,
                "audioConfidence": 0.5,
                "silentScale": 0.1,
                "isSilent": 1,
                "waveformPeakScaled": 0.0,
                "waveformPeakScaledLast": 0.1,
            },
        ]

        summary = c1.summarise_c1_samples(samples, hard_stop_after_seconds=20.0)

        self.assertEqual(summary["postStopSampleCount"], 2)
        self.assertAlmostEqual(summary["postStopFirstIsSilentSeconds"], 0.1)
        self.assertAlmostEqual(summary["postStopFirstSilentScaleBelow0p2Seconds"], 1.2)


if __name__ == "__main__":
    unittest.main()
