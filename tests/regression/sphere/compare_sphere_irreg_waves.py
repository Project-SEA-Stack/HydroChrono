#!/usr/bin/env python3
"""
Sphere Irregular Waves Regression Test Comparison Script

This script compares the results of the sphere irregular waves test against reference data.
"""

import sys
import os
from pathlib import Path

# Add the utilities directory to the path to import the comparison template
sys.path.append(str(Path(__file__).parent.parent.parent / "utilities"))
from compare_template import run_comparison

def main():
    """Main comparison function for sphere irregular waves test."""
    
    # Get the reference data file
    ref_file = Path(__file__).parent.parent.parent / "reference_data" / "sphere" / "irreg_waves" / "hc_ref_sphere_irreg_waves.txt"
    
    # Get the result file from the build directory
    # Look for it in the build directory structure
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent.parent.parent  # Go up to project root
    build_dir = project_root / "build"
    
    # Find the result file
    result_file = build_dir / "bin" / "tests" / "regression" / "sphere" / "results" / "CHRONO_SPHERE_IRREGULAR_WAVES.txt"
    
    if not result_file.exists():
        print(f"Error: Result file not found: {result_file}")
        sys.exit(1)
    
    print(f"Comparing sphere irregular waves test...")
    print(f"  Reference: {ref_file}")
    print(f"  Result:    {result_file}")
    
    # Run comparison using the template
    try:
        n1, n2, passed = run_comparison(
            str(ref_file),
            str(result_file),
            test_name="Sphere Irregular Waves",
            y_label="Heave (m)",
            executable_patterns=["sphere_irreg_waves_test"],
            pass_criteria=(1e-4, 0.02)
        )
        
        if passed:
            print("Sphere irregular waves comparison PASSED")
            sys.exit(0)
        else:
            print("Sphere irregular waves comparison FAILED")
            sys.exit(1)
            
    except Exception as e:
        print(f"ERROR during comparison: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 