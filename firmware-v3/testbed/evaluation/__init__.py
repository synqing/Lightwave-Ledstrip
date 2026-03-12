"""Quality evaluation pipeline for LightwaveOS LED output.

Layers:
    L0 - Frame quality (per-frame pixel validity, saturation, range)
    L1 - Temporal coherence (flicker, jitter, smoothness)
    L2 - Audio-visual alignment (beat sync, energy correlation)
    L3 - Perceptual similarity (1D structural similarity, distribution matching)
    L4 - Aesthetic quality (MLLM-as-judge, rubric scoring) [Phase B]
"""
