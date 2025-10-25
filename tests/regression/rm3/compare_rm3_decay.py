#!/usr/bin/env python3
"""
RM3 Decay Regression Test Comparison

This script compares the RM3 decay test results with reference data and generates
comparison plots using the standardized template.

Usage:
    python compare_decay.py <reference_file> <test_file>
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
        ref_file = os.path.join(os.path.dirname(__file__), "..", "reference_data", "rm3", "decay", "hc_ref_rm3_decay.txt")
        
        # Find the result file
        build_dir = os.environ.get('HYDROCHRONO_BUILD_DIR', 'C:/code/HydroChrono/build')
        test_file = os.path.join(build_dir, "bin", "tests", "regression", "rm3", "results", "CHRONO_RM3_DECAY.txt")
    elif len(sys.argv) == 3:
        ref_file = sys.argv[1]
        test_file = sys.argv[2]
    else:
        print("Usage: python compare_decay.py <reference_file> <test_file>")
        print("   or: python compare_decay.py default")
        sys.exit(1)
    
    # RM3 Decay specific configuration
    test_name = "RM3 Decay Test"
    executable_patterns = ["rm3_decay_test", "rm3_decay_test.exe"]
    
    # Define the columns to plot and their configurations
    test_configs = [
        {
            'column_index': 1,  # Float Heave
            'test_name': f"{test_name} - Float Heave",
            'y_label': "Float Heave (m)",
            'validation_tolerance': (1e-4, 0.02)  # RM3-specific tolerance
        },
        {
            'column_index': 2,  # Spar Heave
            'test_name': f"{test_name} - Spar Heave",
            'y_label': "Spar Heave (m)",
            'validation_tolerance': (1e-4, 0.02)  # RM3-specific tolerance
        }
    ]
    
    try:
        # Load data for additional RM3-specific validations
        ref_data = np.loadtxt(ref_file, skiprows=1)
        test_data = np.loadtxt(test_file, skiprows=1)
        
        # Show where the plots will be saved
        test_file_path = Path(test_file)
        plots_dir = test_file_path.parent / "plots"
        plots_dir.mkdir(parents=True, exist_ok=True)
        print(f"Plots will be saved to: {plots_dir}")

        # Create temporary files with interpolated data to match time steps
        import tempfile
        
        # Interpolate reference data to match test data time steps
        nval = test_data.shape[0]
        x = np.linspace(test_data[0, 0], test_data[nval-1, 0], nval)
        
        # Interpolate the float heave and spar heave data
        floatHeaveRef = np.interp(x, ref_data[:,0], ref_data[:,1])
        sparHeaveRef = np.interp(x, ref_data[:,0], ref_data[:,2])
        
        # Create interpolated reference data
        ref_data_interp = np.column_stack((x, floatHeaveRef, sparHeaveRef))
        
        # Create temporary files with interpolated data
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as temp_ref:
            np.savetxt(temp_ref.name, ref_data_interp, fmt='%.6f')
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as temp_test:
            np.savetxt(temp_test.name, test_data, fmt='%.6f')
        
        try:
            # Override the plots directory to ensure it's saved in the correct location
            import sys as sys_module
            sys_module.path.append(os.path.join(os.path.dirname(__file__), '..'))
            from compare_template import create_comparison_plot, format_path
            
            # Manually create the comparison plot to control the output directory
            for config in test_configs:
                column_index = config['column_index']
                test_name = config['test_name']
                y_label = config['y_label']
                
                # Create data arrays for this column
                ref_col_data = np.column_stack((ref_data_interp[:, 0], ref_data_interp[:, column_index]))
                test_col_data = np.column_stack((test_data[:, 0], test_data[:, column_index]))
                
                # Create the plot in the correct directory
                create_comparison_plot(
                    ref_col_data, test_col_data, test_name, plots_dir,
                    ref_file_path=format_path(ref_file),
                    test_file_path=format_path(test_file),
                    y_label=y_label,
                    executable_patterns=executable_patterns
                )
            
            # Create dummy results for compatibility
            results = [(0.0, 0.0, True)] * len(test_configs)
        finally:
            # Clean up temporary files
            os.unlink(temp_ref.name)
            os.unlink(temp_test.name)
        
        # Additional RM3-specific validations (from the original script)
        nval = test_data.shape[0]
        
        # Resample refData to testData sampling rate
        x = np.linspace(test_data[0, 0], test_data[nval-1, 0], nval)
        
        # Compare the float heave
        floatHeaveRef = np.interp(x, ref_data[:,0], ref_data[:,1])
        floatHeaveTest = np.interp(x, test_data[:,0], test_data[:,1])
        floatHeaveComp = floatHeaveRef - floatHeaveTest

        # Compare the spar heave
        sparHeaveRef = np.interp(x, ref_data[:,0], ref_data[:,2])
        sparHeaveTest = np.interp(x, test_data[:,0], test_data[:,2])
        sparHeaveComp = sparHeaveRef - sparHeaveTest

        # Frobenius norm - Float Heave
        floatHeaven1 = np.linalg.norm(floatHeaveComp)/nval
        # infinity norm - Float Heave
        floatHeaven2 = np.linalg.norm(floatHeaveComp, np.inf)

        # Frobenius norm - Spar Heave
        sparHeaven1 = np.linalg.norm(sparHeaveComp)/nval
        # infinity norm - Spar Heave
        sparHeaven2 = np.linalg.norm(sparHeaveComp, np.inf)
        
        if (floatHeaven1 > 1e-4 or floatHeaven2 > 0.02 or sparHeaven1 > 1e-4 or sparHeaven2 > 0.02):
            print(f"RM3 validation failed: Float heave difference {floatHeaven1:.2e} > 1e-4 or {floatHeaven2:.2e} > 0.02")
            print(f"RM3 validation failed: Spar heave difference {sparHeaven1:.2e} > 1e-4 or {sparHeaven2:.2e} > 0.02")
            sys.exit(1)
        
        # Check if all template comparisons passed
        all_passed = all(result[2] for result in results)
        
        if all_passed:
            print("RM3 DECAY TEST PASSED - All comparisons within tolerance")
            print(f"Generated plots:")
            for config in test_configs:
                plot_name = config['test_name'].lower().replace(' ', '_').replace('-', '_')
                print(f"  - {config['test_name']}: {plots_dir}/{plot_name}_comparison.png")
        else:
            print("RM3 DECAY TEST FAILED - Some comparisons outside tolerance")
            sys.exit(1)
        
    except Exception as e:
        print(f"Error during comparison: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 