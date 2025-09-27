#!/usr/bin/env python3
"""
HydroChrono Sphere Regression Test Comparison

This script compares sphere test results against reference data using the
standardized comparison template.

Usage:
    python compare.py <reference_file> <test_file>
    python compare.py default <test_file>  # Uses default reference data
"""

import sys
import os
from pathlib import Path

# Import the comparison template
sys.path.append(os.path.join(os.path.dirname(__file__), '../utilities'))
from compare_template import run_comparison

if __name__ == '__main__':
    """
    Compare sphere test results with reference data
    """
    if len(sys.argv) != 3:
        print("Usage: python compare.py <reference_file> <test_file>")
        print("       python compare.py default <test_file>  # Uses default reference data")
        sys.exit(1)

    # Use new reference data location - use correct relative path
    default_ref = os.path.join(os.path.dirname(__file__), '../reference_data/sphere/decay/hc_ref_sphere_decay.txt')
    fname_ref = sys.argv[1] if sys.argv[1] != 'default' else default_ref
    fname_rst = sys.argv[2]

    # Show where the plot will be saved
    test_file_path = Path(fname_rst)
    plots_dir = test_file_path.parent / "plots"
    # Ensure the plots directory exists
    plots_dir.mkdir(parents=True, exist_ok=True)
    print(f"Plot will be saved to: {plots_dir}")

    # Sphere-specific configuration
    test_name = "Sphere Decay Test"
    # Convert test_name to lowercase with underscores for filename
    safe_test_name = test_name.lower().replace(' ', '_').replace('-', '_')
    print(f"Plot filename: {plots_dir}/{safe_test_name}_comparison.png")
    y_label = "Heave (m)"
    executable_patterns = ["sphere_decay_test", "sphere_decay"]
    pass_criteria = (1e-4, 0.02)  # (L2 threshold, L-infinity threshold)
    
    # Run the comparison using the template
    n1, n2, passed = run_comparison(
        fname_ref, fname_rst, test_name, y_label, 
        executable_patterns, pass_criteria
    )
    
    sys.exit(0 if passed else 1) 