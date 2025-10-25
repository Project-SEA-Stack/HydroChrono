#!/usr/bin/env python3
"""
Sphere Irregular Waves with ETA Import Regression Test Comparison Script

This script compares the results of the sphere irregular waves with ETA import test against reference data.
Note: This test uses the same reference data as the regular irregular waves test since the output format is identical.
"""

import sys
from pathlib import Path
import os

# Add the utilities directory to the path to import the comparison template
sys.path.append(str(Path(__file__).parent.parent.parent / "utilities"))
from compare_template import run_comparison

def main():
    """Main comparison function for sphere irregular waves with ETA import test."""
    
    # Get the reference data file (same as regular irregular waves)
    ref_file = Path(__file__).parent.parent.parent / "reference_data" / "sphere" / "irreg_waves" / "ref_sphere_irreg_waves.txt"
    
    # Look for CHRONO-named result file in various locations
    build_dir = Path(os.environ.get('HYDROCHRONO_BUILD_DIR', 'C:/code/HydroChrono/build'))
    result_file = build_dir / "bin" / "tests" / "regression" / "sphere" / "results" / "CHRONO_SPHERE_IRREGULAR_WAVES.txt"
    
    if not result_file.exists():
        print(f"Error: Result file not found: {result_file}")
        sys.exit(1)
    
    print(f"Comparing sphere irregular waves with ETA import test...")
    print(f"  Reference: {ref_file}")
    print(f"  Result:    {result_file}")
    
    # Run comparison using the template
    try:
        n1, n2, passed = run_comparison(
            str(ref_file),
            str(result_file),
            test_name="Sphere Irregular Waves with ETA Import",
            y_label="Heave (m)",
            executable_patterns=["sphere_irreg_waves_eta_test"],
            pass_criteria=(1e-4, 0.02)
        )
        
        if passed:
            print("Sphere irregular waves with ETA import comparison PASSED")
            sys.exit(0)
        else:
            print("Sphere irregular waves with ETA import comparison FAILED")
            sys.exit(1)
            
    except Exception as e:
        print(f"ERROR during comparison: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 