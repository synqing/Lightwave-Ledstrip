#!/usr/bin/env python3
"""Quick analysis of benchmark results."""
import pandas as pd
import numpy as np
import sys

if len(sys.argv) < 2:
    print("Usage: python analyze_results.py <results.csv>")
    sys.exit(1)

# Load results
df = pd.read_csv(sys.argv[1])

# Group by pipeline
print('=' * 80)
print('AGGREGATE METRICS BY PIPELINE')
print('=' * 80)

for pipeline in df['pipeline'].unique():
    subset = df[df['pipeline'] == pipeline]
    print(f'\n{pipeline}:')
    print(f'  Banding Score:     mean={subset["banding_score"].mean():.4f}, std={subset["banding_score"].std():.4f}')
    print(f'  Stability Score:   mean={subset["stability_score"].mean():.4f}, std={subset["stability_score"].std():.4f}')
    print(f'  MAE:               mean={subset["mae"].mean():.2f}, std={subset["mae"].std():.2f}')
    print(f'  RMSE:              mean={subset["rmse"].mean():.2f}, std={subset["rmse"].std():.2f}')
    print(f'  Execution Time:    mean={subset["execution_time_ms"].mean():.2f} ms')

print('\n' + '=' * 80)
print('BEST PERFORMERS')
print('=' * 80)

# Best at reducing banding
best_banding = df.loc[df['banding_score'].idxmin()]
print(f'\nLowest Banding: {best_banding["pipeline"]} × {best_banding["stimulus"]}')
print(f'  Score: {best_banding["banding_score"]:.4f}')

# Best stability
best_stability = df.loc[df['stability_score'].idxmax()]
print(f'\nHighest Stability: {best_stability["pipeline"]} × {best_stability["stimulus"]}')
print(f'  Score: {best_stability["stability_score"]:.4f}')

# Best accuracy
best_accuracy = df.loc[df['mae'].idxmin()]
print(f'\nBest Accuracy (Lowest MAE): {best_accuracy["pipeline"]} × {best_accuracy["stimulus"]}')
print(f'  MAE: {best_accuracy["mae"]:.4f}')

print('\n' + '=' * 80)
print('STIMULUS-SPECIFIC ANALYSIS')
print('=' * 80)

for stimulus in df['stimulus'].unique():
    subset = df[df['stimulus'] == stimulus]
    print(f'\n{stimulus}:')
    best_pipeline = subset.loc[subset['banding_score'].idxmin(), 'pipeline']
    best_score = subset['banding_score'].min()
    print(f'  Best pipeline: {best_pipeline} (banding={best_score:.4f})')
