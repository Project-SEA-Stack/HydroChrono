#!/usr/bin/env python3
"""
Sphere Regular Waves Regression Test Comparison Script

This script compares the results of the sphere regular waves test against reference data.
The test runs 10 different wave conditions, so we need to compare each one.
"""

import sys
import os
import glob
from pathlib import Path

# Add the utilities directory to the path to import the comparison template
sys.path.append(str(Path(__file__).parent.parent.parent / "utilities"))
from compare_template import run_comparison, run_multi_column_comparison

def main():
    """Main comparison function for sphere regular waves test."""
    
    # Get the reference data directory
    ref_dir = Path(__file__).parent.parent.parent / "reference_data" / "sphere" / "reg_waves"
    
    # Get the results directory from the build directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent.parent.parent  # Go up to project root
    build_dir = project_root / "build"
    
    # Find the results directory
    results_dir = build_dir / "bin" / "tests" / "regression" / "sphere" / "results"
    
    if not results_dir.exists():
        print(f"Error: Results directory not found: {results_dir}")
        sys.exit(1)
    
    # Find all result files
    result_files = list(results_dir.glob("CHRONO_SPHERE_REG_WAVES_*.txt"))
    result_files.sort()
    
    if not result_files:
        print(f"Error: No result files found in {results_dir}")
        sys.exit(1)
    
    print(f"Found {len(result_files)} result files in {results_dir}")
    
    # Compare each wave condition
    all_passed = True
    
    for result_file in result_files:
        # Extract wave number from filename
        wave_num = result_file.stem.split('_')[-1]
        ref_file = ref_dir / f"hc_ref_sphere_reg_waves_{wave_num}.txt"
        
        if not ref_file.exists():
            print(f"Warning: Reference file {ref_file} not found, skipping wave {wave_num}")
            continue
        
        print(f"\nComparing wave condition {wave_num}...")
        print(f"  Reference: {ref_file}")
        print(f"  Result:    {result_file}")
        
        # Run comparison using the template
        try:
            n1, n2, passed = run_comparison(
                str(ref_file),
                str(result_file),
                test_name=f"Sphere Regular Waves - Wave {wave_num}",
                y_label="Heave (m)",
                executable_patterns=["sphere_reg_waves_test"],
                pass_criteria=(1e-4, 0.02)
            )
            
            if not passed:
                all_passed = False
                print(f"  FAILED Wave {wave_num} comparison")
            else:
                print(f"  PASSED Wave {wave_num} comparison")
                
        except Exception as e:
            print(f"  ERROR comparing wave {wave_num}: {e}")
            all_passed = False
    
    if all_passed:
        print("\nAll wave conditions passed comparison!")
        sys.exit(0)
    else:
        print("\nSome wave conditions failed comparison!")
        sys.exit(1)

if __name__ == "__main__":
    main() 