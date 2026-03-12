# LightwaveOS PyTorch Testbed

A PyTorch-based testbed for generating DIFNO training data from the LightwaveOS firmware's physics engine.

## Overview

This testbed provides:

1. **TransportCore** — PyTorch neural network approximating the firmware's semi-Lagrangian transport equation
2. **CalibrationBridge** — Compares PyTorch outputs against firmware reference pairs
3. **TrainingDataGenerator** — Generates synthetic training pairs for DIFNO (Differentiable Fourier Neural Operators)
4. **TransportDataset** — PyTorch Dataset for easy integration into training pipelines

## Quick Start

### 1. Generate Training Data

```bash
python -m testbed.scripts.generate_training_data --n-pairs 10000
python -m testbed.scripts.generate_training_data --n-pairs 5000 --with-jacobians
```

### 2. Run Calibration

```bash
python -m testbed.scripts.run_calibration --reference data/reference_pairs.npz
```

### 3. Use in Training

```python
from testbed.training import TransportDataset

dataset = TransportDataset("data/training_pairs.npz")
loader = dataset.get_dataloader(batch_size=32, shuffle=True)
```

## Installation

```bash
pip install -e .
```

## Key Components

### CalibrationBridge (`calibration/bridge.py`)
- Loads firmware reference pairs
- Runs PyTorch transport core
- Compares via metrics (L2, energy, tone map)
- Generates pass/fail report

### TrainingDataGenerator (`training/data_generator.py`)
- Samples parameters (Sobol, random, grid)
- Runs through PyTorch transport core
- Optionally computes Jacobians via autograd
- Saves to .npz format

### TransportDataset (`training/dataset.py`)
- Loads .npz training pairs
- Provides PyTorch Dataset interface
- Supports optional Jacobians
- Easy DataLoader integration

## Tests

```bash
pytest tests/ -v
```

## File Structure

```
testbed/
├── __init__.py
├── pyproject.toml
├── README.md
├── core/
│   ├── __init__.py
│   ├── transport_core.py          (agent: @transport)
│   ├── render_utils.py            (agent: @render)
│   └── context.py                 (agent: @context)
├── calibration/
│   ├── __init__.py
│   ├── bridge.py
│   └── metrics.py
├── training/
│   ├── __init__.py
│   ├── data_generator.py
│   └── dataset.py
├── data/
│   └── .gitkeep
├── tests/
│   ├── __init__.py
│   ├── test_transport_core.py
│   ├── test_render_utils.py
│   ├── test_context.py
│   └── test_calibration.py
└── scripts/
    ├── run_calibration.py
    └── generate_training_data.py
```

## Calibration Metrics

1. Per-pixel L2 error: < 0.01
2. Energy divergence: < 1.0%
3. Tone map agreement: > 99% (±1 LSB)
4. Cosine similarity (informational)

## Dependencies

- torch >= 2.0
- numpy
- scipy
- tqdm
- pytest

See pyproject.toml for complete requirements.
