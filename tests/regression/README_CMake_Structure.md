# CMake Test Structure Documentation

## Overview

The HydroChrono test system is organized into two distinct test suites that can be run independently or together:

1. **Legacy Tests** (24 tests) - Original demo-based tests
2. **Regression Tests** (2 tests) - New framework-based tests

## CMake File Structure

```
CMakeLists.txt                    # Main project file
├── tests/
│   ├── CMakeLists.txt           # Legacy tests configuration
│   └── regression/
│       ├── CMakeLists.txt       # Regression tests configuration
│       ├── sphere/
│       │   ├── demo_sphere_decay.cpp
│       │   └── compare.py
│       └── reference_data/
│           └── sphere/decay/
│               └── sphere_decay_hc_data.txt
```

## Key CMake Variables

### Environment Configuration (Main CMakeLists.txt)
```cmake
# Defined in main CMakeLists.txt, passed to all test subdirectories
set(CHRONO_DLL_DIR "${Chrono_DIR}/../bin/Release")
set(IRRLICHT_DLL_DIR "C:/libs/irrlicht-1.8.4/bin/Win64-VisualStudio")
set(TEST_ENVIRONMENT "PATH=${CHRONO_DLL_DIR};${IRRLICHT_DLL_DIR};$ENV{PATH}")
```

### Test Labels
- **Legacy Tests:** `demos`, `core`, `medium`, `long`, `ref`
- **Regression Tests:** `regression`, `sphere`, `decay`, `small`, `core`, `reference`

## Test Execution Commands

### Full Test Suite (26 tests)
```bash
ctest -C Release
```

### Only Regression Tests (2 tests)
```bash
ctest -C Release -L regression
```

### Only Legacy Tests (24 tests)
```bash
ctest -C Release -LE regression
```

### Specific Test Categories
```bash
# Quick tests only
ctest -C Release -L "small"

# Reference comparison tests only
ctest -C Release -L "reference"

# Sphere-related tests only
ctest -C Release -L "sphere"
```

## Test Organization

### Legacy Tests (tests/CMakeLists.txt)
- **Location:** `tests/CMakeLists.txt`
- **Structure:** Demo-based tests using existing demo executables
- **Labels:** `demos`, `core`, `medium`, `long`, `ref`
- **Environment:** Uses `TEST_ENVIRONMENT` from parent scope

### Regression Tests (tests/regression/CMakeLists.txt)
- **Location:** `tests/regression/CMakeLists.txt`
- **Structure:** Dedicated test executables with centralized reference data
- **Labels:** `regression`, `sphere`, `decay`, `small`, `core`, `reference`
- **Environment:** Uses `TEST_ENVIRONMENT` from parent scope

## Adding New Tests

### Adding a New Regression Test

1. **Create test directory:**
   ```
   tests/regression/new_test_case/
   ├── demo_new_test_case.cpp
   └── compare.py
   ```

2. **Add reference data:**
   ```
   tests/regression/reference_data/new_test_case/
   └── new_test_case_data.txt
   ```

3. **Update tests/regression/CMakeLists.txt:**
   ```cmake
   # New Test Case
   add_executable(new_test_case_test)
   set_target_properties(new_test_case_test
       PROPERTIES
       RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/regression/Release/new_test_case
   )
   
   target_sources(new_test_case_test PRIVATE
       ${CMAKE_CURRENT_SOURCE_DIR}/new_test_case/demo_new_test_case.cpp
   )
   
   target_link_libraries(new_test_case_test PRIVATE HydroChrono HydroChronoGUI)
   
   # Register test
   add_test(NAME new_test_case_regression
       COMMAND ${CMAKE_BINARY_DIR}/tests/regression/Release/new_test_case/new_test_case_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
   )
   
   set_tests_properties(new_test_case_regression
       PROPERTIES 
       LABELS "regression;new_test_case;small;core"
       ENVIRONMENT "${TEST_ENVIRONMENT}"
   )
   ```

### Adding a New Legacy Test

1. **Add to tests/CMakeLists.txt:**
   ```cmake
   if(TARGET demo_new_test)
       add_test(NAME demo_new_test_01
           COMMAND $<TARGET_FILE:demo_new_test> ${HYDROCHRONO_DATA_DIR} --nogui
       )
       set_tests_properties(demo_new_test_01
           PROPERTIES LABELS "demos;medium;core"
           ENVIRONMENT "${TEST_ENVIRONMENT}"
       )
   endif()
   ```

## Maintenance Notes

### Environment Variables
- All test environment variables are defined in the main `CMakeLists.txt`
- They are automatically passed to all test subdirectories
- No need to redefine in individual test files

### Test Labels
- Use consistent labeling for easy filtering
- `regression` label is reserved for new framework tests
- `demos` label is used for legacy tests

### File Organization
- Regression tests use centralized reference data in `tests/regression/reference_data/`
- Legacy tests use reference data in their respective demo directories
- Test executables are built in separate directories to avoid conflicts

## Troubleshooting

### Common Issues

1. **DLL Not Found Errors:**
   - Ensure `TEST_ENVIRONMENT` includes correct paths
   - Check that `Chrono_DIR` and Irrlicht paths are correct

2. **Test Not Found by Label:**
   - Verify test has correct `LABELS` property
   - Check that `add_test()` and `set_tests_properties()` are in same scope

3. **Python Module Not Found:**
   - Ensure `Python3_EXECUTABLE` points to correct environment
   - Check that numpy and matplotlib are installed in that environment

### Debugging Commands

```bash
# List all tests with their labels
ctest -C Release -N

# List tests with specific label
ctest -C Release -L regression -N

# Run with verbose output
ctest -C Release -L regression --output-on-failure -V
``` 