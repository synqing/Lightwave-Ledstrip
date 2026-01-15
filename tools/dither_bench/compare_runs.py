#!/usr/bin/env python3
"""
Comparative analysis across multiple benchmark runs.
"""
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import sys

def load_all_results(report_dirs):
    """Load and combine results from multiple runs."""
    all_data = []
    for report_dir in report_dirs:
        csv_path = Path(report_dir) / 'results.csv'
        if csv_path.exists():
            df = pd.read_csv(csv_path)
            df['run_name'] = Path(report_dir).name
            all_data.append(df)
    return pd.concat(all_data, ignore_index=True)

def analyze_consistency(combined_df):
    """Analyze consistency across different LED counts and frame counts."""
    print('=' * 80)
    print('CROSS-RUN CONSISTENCY ANALYSIS')
    print('=' * 80)
    
    # Group by pipeline and stimulus
    for pipeline in combined_df['pipeline'].unique():
        print(f'\n{pipeline}:')
        pipeline_data = combined_df[combined_df['pipeline'] == pipeline]
        
        # Banding consistency
        banding_mean = pipeline_data['banding_score'].mean()
        banding_std = pipeline_data['banding_score'].std()
        print(f'  Banding Score:     μ={banding_mean:.4f}, σ={banding_std:.4f} (CV={banding_std/banding_mean:.2%})')
        
        # Stability consistency
        stability_mean = pipeline_data['stability_score'].mean()
        stability_std = pipeline_data['stability_score'].std()
        print(f'  Stability Score:   μ={stability_mean:.4f}, σ={stability_std:.4f}')
        
        # MAE consistency
        mae_mean = pipeline_data['mae'].mean()
        mae_std = pipeline_data['mae'].std()
        print(f'  MAE:               μ={mae_mean:.2f}, σ={mae_std:.2f} (CV={mae_std/mae_mean:.2%})')

def create_comparison_plots(combined_df, output_dir):
    """Create visualization comparing all runs."""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Plot 1: Banding score by pipeline and stimulus
    fig, ax = plt.subplots(figsize=(14, 8))
    
    pipelines = combined_df['pipeline'].unique()
    stimuli = combined_df['stimulus'].unique()
    
    x = np.arange(len(stimuli))
    width = 0.15
    
    for i, pipeline in enumerate(pipelines):
        pipeline_data = combined_df[combined_df['pipeline'] == pipeline]
        means = [pipeline_data[pipeline_data['stimulus'] == s]['banding_score'].mean() 
                for s in stimuli]
        stds = [pipeline_data[pipeline_data['stimulus'] == s]['banding_score'].std() 
               for s in stimuli]
        
        ax.bar(x + i * width, means, width, label=pipeline, yerr=stds, capsize=3)
    
    ax.set_xlabel('Stimulus')
    ax.set_ylabel('Banding Score (lower is better)')
    ax.set_title('Banding Performance by Pipeline and Stimulus (All Runs)')
    ax.set_xticks(x + width * (len(pipelines) - 1) / 2)
    ax.set_xticklabels(stimuli, rotation=45, ha='right')
    ax.legend()
    ax.grid(axis='y', alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'banding_comparison.png', dpi=150)
    plt.close()
    
    # Plot 2: Stability vs Banding trade-off
    fig, ax = plt.subplots(figsize=(10, 8))
    
    for pipeline in pipelines:
        pipeline_data = combined_df[combined_df['pipeline'] == pipeline]
        ax.scatter(pipeline_data['banding_score'], 
                  pipeline_data['stability_score'],
                  label=pipeline, alpha=0.6, s=100)
    
    ax.set_xlabel('Banding Score (lower is better)')
    ax.set_ylabel('Stability Score (higher is better)')
    ax.set_title('Banding vs Stability Trade-off (All Runs)')
    ax.legend()
    ax.grid(alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'tradeoff_analysis.png', dpi=150)
    plt.close()
    
    print(f'\n✓ Plots saved to: {output_dir}')

def main():
    if len(sys.argv) < 2:
        print("Usage: python compare_runs.py <report_dir1> [report_dir2] ...")
        sys.exit(1)
    
    report_dirs = sys.argv[1:]
    
    # Load all results
    combined_df = load_all_results(report_dirs)
    
    print(f'Loaded {len(combined_df)} test results from {len(report_dirs)} runs')
    print(f'Runs: {combined_df["run_name"].unique()}')
    
    # Analyze consistency
    analyze_consistency(combined_df)
    
    # Create plots
    create_comparison_plots(combined_df, 'reports/comparative_analysis')
    
    # Save combined dataset
    combined_path = Path('reports/comparative_analysis/combined_results.csv')
    combined_df.to_csv(combined_path, index=False)
    print(f'\n✓ Combined dataset saved to: {combined_path}')

if __name__ == '__main__':
    main()
