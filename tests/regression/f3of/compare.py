#!/usr/bin/env python3
"""
F3OF DT3 Regression Test Comparison

This script compares the F3OF DT3 test results with reference data and generates
comparison plots for flap fore pitch and flap aft pitch using the standardized template.

Usage:
    python compare.py <reference_file> <test_file>
"""

import sys
import os
from pathlib import Path
import numpy as np

# Import the comparison template
sys.path.append(os.path.join(os.path.dirname(__file__), '../utilities'))
from compare_template import run_multi_column_comparison

def main():
    if len(sys.argv) == 1 or (len(sys.argv) == 3 and sys.argv[1] == 'default'):
        # Use default reference and result file locations
        ref_file = os.path.join(os.path.dirname(__file__), "..", "reference_data", "f3of", "dt3", "hc_ref_f3of_dt3_flap_pitch.txt")
        build_dir = os.environ.get('HYDROCHRONO_BUILD_DIR', os.path.join(os.path.dirname(__file__), '..', '..', '..', 'build'))
        test_file = os.path.join(build_dir, "bin", "tests", "regression", "f3of", "results", "CHRONO_F3OF_DT3_FLAP_PITCH.txt")
    elif len(sys.argv) == 3:
        ref_file = sys.argv[1]
        test_file = sys.argv[2]
    else:
        print("Usage: python compare.py <reference_file> <test_file>")
        print("   or: python compare.py default")
        sys.exit(1)
    
    # F3OF DT3 specific configuration
    test_name = "F3OF DT3 Decay Test"
    executable_patterns = ["f3of_dt3_test", "f3of_dt3_test.exe"]
    
    # Define the columns to plot and their configurations
    test_configs = [
        {
            'column_index': 3,  # Flap Fore Pitch
            'test_name': f"{test_name} - Flap Fore Pitch",
            'y_label': "Flap Fore Pitch (radians)",
            'validation_tolerance': (1e-6, 1e-6)  # F3OF-specific tolerance
        },
        {
            'column_index': 4,  # Flap Aft Pitch
            'test_name': f"{test_name} - Flap Aft Pitch", 
            'y_label': "Flap Aft Pitch (radians)",
            'validation_tolerance': (1e-6, 1e-6)  # F3OF-specific tolerance
        }
    ]
    
    try:
        # Load data for additional F3OF-specific validations
        ref_data = np.loadtxt(ref_file, skiprows=1)
        test_data = np.loadtxt(test_file, skiprows=1)
        
        # Show where the plots will be saved
        test_file_path = Path(test_file)
        plots_dir = test_file_path.parent / "plots"
        plots_dir.mkdir(parents=True, exist_ok=True)
        print(f"Plots will be saved to: {plots_dir}")

        # Run the multi-column comparison using the template
        results = run_multi_column_comparison(
            ref_file, test_file, test_configs, 
            executable_patterns=executable_patterns
        )
        
        # Additional F3OF-specific validations (from the original script)
        
        # Check base surge (column 1)
        diff_base_surge = np.linalg.norm(ref_data[:,1] - test_data[:,1]) / len(ref_data[:,1])
        if diff_base_surge > 1e-6:
            print(f"F3OF validation failed: Base surge difference {diff_base_surge:.2e} > 1e-6")
            sys.exit(1)
        
        # Check base pitch (column 2) - more stringent tolerance
        diff_base_pitch = np.linalg.norm(ref_data[:,2] - test_data[:,2]) / len(ref_data[:,2])
        if diff_base_pitch > 1e-10:
            print(f"F3OF validation failed: Base pitch difference {diff_base_pitch:.2e} > 1e-10")
            sys.exit(1)
        
        # Check flap fore pitch (column 3)
        diff_flap_fore_pitch = np.linalg.norm(ref_data[:,3] - test_data[:,3]) / len(ref_data[:,3])
        if diff_flap_fore_pitch > 1e-6:
            print(f"F3OF validation failed: Flap fore pitch difference {diff_flap_fore_pitch:.2e} > 1e-6")
            sys.exit(1)
        
        # Check flap aft pitch (column 4)
        diff_flap_aft_pitch = np.linalg.norm(ref_data[:,4] - test_data[:,4]) / len(ref_data[:,4])
        if diff_flap_aft_pitch > 1e-6:
            print(f"F3OF validation failed: Flap aft pitch difference {diff_flap_aft_pitch:.2e} > 1e-6")
            sys.exit(1)
        
        # Check if all template comparisons passed
        all_passed = all(result[2] for result in results)
        
        if all_passed:
            print("F3OF DT3 TEST PASSED - All comparisons within tolerance")
            print(f"Generated plots:")
            for config in test_configs:
                plot_name = config['test_name'].lower().replace(' ', '_').replace('-', '_')
                print(f"  - {config['test_name']}: {plots_dir}/{plot_name}_comparison.png")
        else:
            print("F3OF DT3 TEST FAILED - Some comparisons outside tolerance")
            sys.exit(1)
        
    except Exception as e:
        print(f"Error during comparison: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()