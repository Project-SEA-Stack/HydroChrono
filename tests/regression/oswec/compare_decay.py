#!/usr/bin/env python3
"""
OSWEC Decay Regression Test Comparison

This script compares the OSWEC decay test results with reference data and generates
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
        ref_file = os.path.join(os.path.dirname(__file__), "..", "reference_data", "oswec", "decay", "hc_ref_oswec_decay.txt")
        
        # Find the result file
        build_dir = os.environ.get('HYDROCHRONO_BUILD_DIR', 'C:/code/HydroChrono/build')
        test_file = os.path.join(build_dir, "bin", "tests", "regression", "oswec", "results", "CHRONO_OSWEC_DECAY.txt")
    elif len(sys.argv) == 3:
        ref_file = sys.argv[1]
        test_file = sys.argv[2]
    else:
        print("Usage: python compare_decay.py <reference_file> <test_file>")
        print("   or: python compare_decay.py default")
        sys.exit(1)
    
    # OSWEC Decay specific configuration
    test_name = "OSWEC Decay Test"
    executable_patterns = ["oswec_decay_test", "oswec_decay_test.exe"]
    
    # Define the columns to plot and their configurations
    test_configs = [
        {
            'column_index': 1,  # Flap Pitch (radians)
            'test_name': f"{test_name} - Flap Pitch",
            'y_label': "Flap Pitch (radians)",
            'validation_tolerance': (1e-4, 0.02)  # OSWEC-specific tolerance
        }
    ]
    
    try:
        # Load data for additional OSWEC-specific validations
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
        
        # Interpolate the flap pitch data (column 1 in reference, column 1 in test)
        flapPitchRef = np.interp(x, ref_data[:,0], ref_data[:,1])
        
        # Create interpolated reference data
        ref_data_interp = np.column_stack((x, flapPitchRef))
        
        # Create temporary files with interpolated data
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as temp_ref:
            np.savetxt(temp_ref.name, ref_data_interp, fmt='%.6f')
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as temp_test:
            # Use only time and flap pitch columns from test data
            test_data_interp = np.column_stack((test_data[:,0], test_data[:,1]))
            np.savetxt(temp_test.name, test_data_interp, fmt='%.6f')
        
        try:
            # Run the multi-column comparison using the template with interpolated data
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
                test_col_data = np.column_stack((test_data_interp[:, 0], test_data_interp[:, column_index]))
                
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
        
        # Additional OSWEC-specific validations (from the original script)
        nval = test_data.shape[0]
        
        # Resample refData to testData sampling rate
        x = np.linspace(test_data[0, 0], test_data[nval-1, 0], nval)
        flapPitchRef = np.interp(x, ref_data[:,0], ref_data[:,1])
        flapPitchTest = np.interp(x, test_data[:,0], test_data[:,1])
        flapPitchComp = flapPitchRef - flapPitchTest

        # Frobenius norm - Flap pitch
        flapPitchn1 = np.linalg.norm(flapPitchComp)/nval
        # infinity norm - Flap pitch
        flapPitchn2 = np.linalg.norm(flapPitchComp, np.inf)
        
        if (flapPitchn1 > 1e-4 or flapPitchn2 > 0.02):
            print(f"OSWEC validation failed: Flap pitch difference {flapPitchn1:.2e} > 1e-4 or {flapPitchn2:.2e} > 0.02")
            sys.exit(1)
        
        # Check if all template comparisons passed
        all_passed = all(result[2] for result in results)
        
        if all_passed:
            print("OSWEC DECAY TEST PASSED - All comparisons within tolerance")
            print(f"Generated plots:")
            for config in test_configs:
                plot_name = config['test_name'].lower().replace(' ', '_').replace('-', '_')
                print(f"  - {config['test_name']}: {plots_dir}/{plot_name}_comparison.png")
        else:
            print("OSWEC DECAY TEST FAILED - Some comparisons outside tolerance")
            sys.exit(1)
        
    except Exception as e:
        print(f"Error during comparison: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 