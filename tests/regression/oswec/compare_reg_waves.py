#!/usr/bin/env python3
"""
OSWEC Regular Waves Regression Test Comparison (Batch Mode)

This script compares all OSWEC regular waves test results with reference data and generates
comparison plots using the standardized template. It loops over all available result files.

Usage:
    python compare_reg_waves.py
"""

import sys
import os
from pathlib import Path
import numpy as np
import glob
import re

# Import the comparison template
sys.path.append(os.path.join(os.path.dirname(__file__), '../utilities'))
from compare_template import create_comparison_plot, format_path

def main():
    # Directories
    script_dir = Path(__file__).parent
    ref_dir = script_dir.parent / "reference_data" / "oswec" / "reg_waves"
    # Default build results dir
    build_dir = Path(os.environ.get('HYDROCHRONO_BUILD_DIR', 'C:/code/HydroChrono/build'))
    # New recommended layout
    results_dir = build_dir / "bin" / "tests" / "regression" / "oswec" / "results"
    if not results_dir.exists():
        # Fallback to older Release layout used previously
        results_dir = build_dir / "tests" / "regression" / "Release" / "oswec" / "results" / "oswec" / "regular_waves"
    if not results_dir.exists():
        print(f"Results directory not found: {results_dir}")
        sys.exit(1)

    # Find all result files
    all_files = [p for p in results_dir.glob("CHRONO_OSWEC_REG_WAVES_*.txt") if not p.stem.endswith("DURATION")]
    def wave_num_from_stem(stem: str):
        m = re.search(r"_(\d+)$", stem)
        return int(m.group(1)) if m else -1
    result_files = sorted(all_files, key=lambda p: wave_num_from_stem(p.stem))
    if not result_files:
        print(f"No result files found in {results_dir}")
        sys.exit(1)

    # Output directory for plots
    plots_dir = results_dir / "plots"
    plots_dir.mkdir(parents=True, exist_ok=True)
    print(f"Plots will be saved to: {plots_dir}")

    # Loop over all result files
    for result_file in result_files:
        wave_num = result_file.stem.split('_')[-1]
        ref_file = ref_dir / f"hc_ref_oswec_reg_waves_{wave_num}.txt"
        if not ref_file.exists():
            print(f"[WARNING] Reference file not found for wave {wave_num}: {ref_file}. Skipping.")
            continue
        print(f"\nComparing wave {wave_num}...")
        print(f"  Reference: {ref_file}")
        print(f"  Result:    {result_file}")
        try:
            # Load data (skip headers)
            ref_data = np.loadtxt(ref_file, skiprows=5)
            test_data = np.loadtxt(result_file, skiprows=5)
            # Prepare data for plotting (Pitch is column 1)
            ref_col_data = np.column_stack((ref_data[:, 0], ref_data[:, 1]))
            test_col_data = np.column_stack((test_data[:, 0], test_data[:, 1]))
            test_name = f"OSWEC Regular Waves Test - Pitch - Wave {wave_num}"
            y_label = "Pitch (radians)"
            executable_patterns = ["oswec_reg_waves_test", "oswec_reg_waves_test.exe"]
            create_comparison_plot(
                ref_col_data, test_col_data, test_name, plots_dir,
                ref_file_path=format_path(str(ref_file)),
                test_file_path=format_path(str(result_file)),
                y_label=y_label,
                executable_patterns=executable_patterns
            )
            print(f"  [OK] Plot generated: {plots_dir}/{test_name.lower().replace(' ', '_').replace('-', '_')}_comparison.png")
        except Exception as e:
            print(f"  [ERROR] Failed to compare wave {wave_num}: {e}")

if __name__ == "__main__":
    main() 