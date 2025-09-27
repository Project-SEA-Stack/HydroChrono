find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Helper function to set up common test target properties
function(setup_test_target TARGET_NAME SRC_DIR RELEASE_DIR)
    set_target_properties(${TARGET_NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${RELEASE_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${RELEASE_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${RELEASE_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/regression/build
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/regression/build
        PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/regression/build
    )

    if(HYDROCHRONO_ENABLE_IRRLICHT)
        target_compile_definitions(${TARGET_NAME} 
            PRIVATE 
            HYDROCHRONO_HAVE_IRRLICHT=1
            "CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
        )
    endif()

    target_include_directories(${TARGET_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/gui
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            HydroChrono
            HydroChronoGUI
    )
endfunction()

# Helper function to copy test data to release directory
function(copy_test_data SRC_DIR RELEASE_DIR)
    if(EXISTS ${SRC_DIR}/hydroData)
        file(COPY ${SRC_DIR}/hydroData DESTINATION ${RELEASE_DIR})
    endif()
    if(EXISTS ${SRC_DIR}/geometry)
        file(COPY ${SRC_DIR}/geometry DESTINATION ${RELEASE_DIR})
    endif()
    if(EXISTS ${SRC_DIR}/eta)
        file(COPY ${SRC_DIR}/eta DESTINATION ${RELEASE_DIR})
    endif()
endfunction()

# --------------------------------------------------
# Define base directory for regression test binaries
# --------------------------------------------------
set(TEST_REGRESSION_OUTPUT_DIR ${HYDROCHRONO_TEST_OUTPUT_DIR}/regression)

# Sphere Decay Regression Test
# ===========================

# Create executable in model-based directory
add_executable(sphere_decay_test)

set(SPHERE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sphere)
set(SPHERE_RELEASE_DIR ${TEST_REGRESSION_OUTPUT_DIR}/sphere)
set(SPHERE_RESULTS_DIR ${SPHERE_RELEASE_DIR}/results)

target_sources(
    sphere_decay_test
    PRIVATE
        ${SPHERE_SRC_DIR}/demo_sphere_decay.cpp
)

setup_test_target(sphere_decay_test ${SPHERE_SRC_DIR} ${SPHERE_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(sphere_decay_test PROPERTIES FOLDER "regression/sphere")

# Register as CTest test
if(TARGET sphere_decay_test)
    # Copy test data to the output directory
    copy_test_data(${SPHERE_SRC_DIR} ${SPHERE_RELEASE_DIR})
    
    add_test(
        NAME sphere_decay_regression
        COMMAND ${SPHERE_RELEASE_DIR}/sphere_decay_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        sphere_decay_regression
        PROPERTIES 
        LABELS "regression;sphere;decay;small;core"
        FIXTURES_SETUP sphere_decay_regression_file
        WORKING_DIRECTORY ${SPHERE_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test - use the new centralized reference data location
    set(FILE_RST ${SPHERE_RESULTS_DIR}/CHRONO_SPHERE_DECAY_HEAVE.txt)
    set(FILE_REF ${CMAKE_SOURCE_DIR}/tests/regression/reference_data/sphere/decay/sphere_decay_hc_data.txt)

    add_test(
        NAME sphere_decay_regression_ref
        COMMAND python compare.py ${FILE_REF} ${FILE_RST}
    )
    
    set_tests_properties(
        sphere_decay_regression_ref
        PROPERTIES 
        LABELS "regression;sphere;decay;reference;core"
        FIXTURES_REQUIRED sphere_decay_regression_file
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/regression/sphere
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# F3OF DT1 Regression Test
# ========================

# Create executable in model-based directory
add_executable(f3of_dt1_test)

set(F3OF_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/f3of)
set(F3OF_RELEASE_DIR ${TEST_REGRESSION_OUTPUT_DIR}/f3of)
set(F3OF_RESULTS_DIR ${F3OF_RELEASE_DIR}/results)

target_sources(
    f3of_dt1_test
    PRIVATE
        ${F3OF_SRC_DIR}/f3of_dt1_test.cpp
)

setup_test_target(f3of_dt1_test ${F3OF_SRC_DIR} ${F3OF_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(f3of_dt1_test PROPERTIES FOLDER "regression/f3of")

# Register as CTest test
if(TARGET f3of_dt1_test)
    # Copy test data to the output directory
    copy_test_data(${F3OF_SRC_DIR} ${F3OF_RELEASE_DIR})
    
    add_test(
        NAME f3of_dt1_regression
        COMMAND ${F3OF_RELEASE_DIR}/f3of_dt1_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        f3of_dt1_regression
        PROPERTIES 
        LABELS "regression;f3of;dt1;decay;core"
        FIXTURES_SETUP f3of_dt1_regression_file
        WORKING_DIRECTORY ${F3OF_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test - use the new centralized reference data location
    set(FILE_RST ${F3OF_RESULTS_DIR}/CHRONO_F3OF_DT1_SURGE.txt)
    set(FILE_REF ${CMAKE_SOURCE_DIR}/tests/regression/reference_data/f3of/dt1/hc_ref_f3of_dt1_surge.txt)

    add_test(
        NAME f3of_dt1_regression_ref
        COMMAND python ${CMAKE_SOURCE_DIR}/tests/regression/f3of/compare_dt1.py
    )
    
    set_tests_properties(
        f3of_dt1_regression_ref
        PROPERTIES 
        LABELS "regression;f3of;dt1;decay;reference;core"
        FIXTURES_REQUIRED f3of_dt1_regression_file
        WORKING_DIRECTORY ${F3OF_RELEASE_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# F3OF DT2 Regression Test
# ========================

# Create executable in model-based directory
add_executable(f3of_dt2_test)

target_sources(
    f3of_dt2_test
    PRIVATE
        ${F3OF_SRC_DIR}/f3of_dt2_test.cpp
)

setup_test_target(f3of_dt2_test ${F3OF_SRC_DIR} ${F3OF_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(f3of_dt2_test PROPERTIES FOLDER "regression/f3of")

# Register as CTest test
if(TARGET f3of_dt2_test)
    # Copy test data to the output directory
    copy_test_data(${F3OF_SRC_DIR} ${F3OF_RELEASE_DIR})
    
    add_test(
        NAME f3of_dt2_regression
        COMMAND ${F3OF_RELEASE_DIR}/f3of_dt2_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        f3of_dt2_regression
        PROPERTIES 
        LABELS "regression;f3of;dt2;decay;core"
        FIXTURES_SETUP f3of_dt2_regression_file
        WORKING_DIRECTORY ${F3OF_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test - use the new centralized reference data location
    set(FILE_RST ${F3OF_RESULTS_DIR}/CHRONO_F3OF_DT2_PITCH.txt)
    set(FILE_REF ${CMAKE_SOURCE_DIR}/tests/regression/reference_data/f3of/dt2/hc_ref_f3of_dt2_pitch.txt)

    add_test(
        NAME f3of_dt2_regression_ref
        COMMAND python ${CMAKE_SOURCE_DIR}/tests/regression/f3of/compare_dt2.py
    )
    
    set_tests_properties(
        f3of_dt2_regression_ref
        PROPERTIES 
        LABELS "regression;f3of;dt2;decay;reference;core"
        FIXTURES_REQUIRED f3of_dt2_regression_file
        WORKING_DIRECTORY ${F3OF_RELEASE_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# F3OF DT3 Regression Test
# ========================

# Create executable in model-based directory
add_executable(f3of_dt3_test)

target_sources(
    f3of_dt3_test
    PRIVATE
        ${F3OF_SRC_DIR}/f3of_dt3_test.cpp
)

setup_test_target(f3of_dt3_test ${F3OF_SRC_DIR} ${F3OF_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(f3of_dt3_test PROPERTIES FOLDER "regression/f3of")

# Register as CTest test
if(TARGET f3of_dt3_test)
    # Copy test data to the output directory
    copy_test_data(${F3OF_SRC_DIR} ${F3OF_RELEASE_DIR})
    
    add_test(
        NAME f3of_dt3_regression
        COMMAND ${F3OF_RELEASE_DIR}/f3of_dt3_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        f3of_dt3_regression
        PROPERTIES 
        LABELS "regression;f3of;dt3;decay;core"
        FIXTURES_SETUP f3of_dt3_regression_file
        WORKING_DIRECTORY ${F3OF_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test - use the new centralized reference data location
    set(FILE_RST ${F3OF_RESULTS_DIR}/CHRONO_F3OF_DT3_FLAP_PITCH.txt)
    set(FILE_REF ${CMAKE_SOURCE_DIR}/tests/regression/reference_data/f3of/dt3/hc_ref_f3of_dt3_flap_pitch.txt)

    add_test(
        NAME f3of_dt3_regression_ref
        COMMAND python ${CMAKE_SOURCE_DIR}/tests/regression/f3of/compare_dt3.py
    )
    
    set_tests_properties(
        f3of_dt3_regression_ref
        PROPERTIES 
        LABELS "regression;f3of;dt3;decay;reference;core"
        FIXTURES_REQUIRED f3of_dt3_regression_file
        WORKING_DIRECTORY ${F3OF_RELEASE_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# Sphere Regular Waves Regression Test
# ===================================

# Create executable in model-based directory
add_executable(sphere_reg_waves_test)

set(SPHERE_REG_WAVES_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sphere/reg_waves)
set(SPHERE_REG_WAVES_RESULTS_DIR ${SPHERE_RESULTS_DIR}/reg_waves)

target_sources(
    sphere_reg_waves_test
    PRIVATE
        ${SPHERE_REG_WAVES_SRC_DIR}/sphere_reg_waves_test.cpp
)

setup_test_target(sphere_reg_waves_test ${SPHERE_REG_WAVES_SRC_DIR} ${SPHERE_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(sphere_reg_waves_test PROPERTIES FOLDER "regression/sphere")

# Register as CTest test
if(TARGET sphere_reg_waves_test)
    # Copy test data to the output directory (only once at top level)
    # copy_test_data(${SPHERE_SRC_DIR} ${SPHERE_RELEASE_DIR})
    
    add_test(
        NAME sphere_reg_waves_regression
        COMMAND ${SPHERE_RELEASE_DIR}/sphere_reg_waves_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        sphere_reg_waves_regression
        PROPERTIES 
        LABELS "regression;sphere;reg_waves;core"
        FIXTURES_SETUP sphere_reg_waves_regression_file
        WORKING_DIRECTORY ${SPHERE_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test
    add_test(
        NAME sphere_reg_waves_regression_ref
        COMMAND python ${CMAKE_SOURCE_DIR}/tests/regression/sphere/reg_waves/compare.py
    )
    
    set_tests_properties(
        sphere_reg_waves_regression_ref
        PROPERTIES 
        LABELS "regression;sphere;reg_waves;reference;core"
        FIXTURES_REQUIRED sphere_reg_waves_regression_file
        WORKING_DIRECTORY ${SPHERE_RELEASE_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# Sphere Irregular Waves Regression Test
# =====================================

# Create executable in model-based directory
add_executable(sphere_irreg_waves_test)

set(SPHERE_IRREG_WAVES_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sphere/irreg_waves)
set(SPHERE_IRREG_WAVES_RESULTS_DIR ${SPHERE_RESULTS_DIR}/irreg_waves)

target_sources(
    sphere_irreg_waves_test
    PRIVATE
        ${SPHERE_IRREG_WAVES_SRC_DIR}/sphere_irreg_waves_test.cpp
)

setup_test_target(sphere_irreg_waves_test ${SPHERE_IRREG_WAVES_SRC_DIR} ${SPHERE_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(sphere_irreg_waves_test PROPERTIES FOLDER "regression/sphere")

# Register as CTest test
if(TARGET sphere_irreg_waves_test)
    # Copy test data to the output directory (only once at top level)
    # copy_test_data(${SPHERE_SRC_DIR} ${SPHERE_RELEASE_DIR})
    
    add_test(
        NAME sphere_irreg_waves_regression
        COMMAND ${SPHERE_RELEASE_DIR}/sphere_irreg_waves_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        sphere_irreg_waves_regression
        PROPERTIES 
        LABELS "regression;sphere;irreg_waves;core"
        FIXTURES_SETUP sphere_irreg_waves_regression_file
        WORKING_DIRECTORY ${SPHERE_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test
    add_test(
        NAME sphere_irreg_waves_regression_ref
        COMMAND python ${CMAKE_SOURCE_DIR}/tests/regression/sphere/irreg_waves/compare.py
    )
    
    set_tests_properties(
        sphere_irreg_waves_regression_ref
        PROPERTIES 
        LABELS "regression;sphere;irreg_waves;reference;core"
        FIXTURES_REQUIRED sphere_irreg_waves_regression_file
        WORKING_DIRECTORY ${SPHERE_RELEASE_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# Sphere Irregular Waves with ETA Import Regression Test
# =====================================================

# Create executable in model-based directory
add_executable(sphere_irreg_waves_eta_test)

set(SPHERE_IRREG_WAVES_ETA_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sphere/irreg_waves_eta)
set(SPHERE_IRREG_WAVES_ETA_RESULTS_DIR ${SPHERE_RESULTS_DIR}/irreg_waves_eta)

target_sources(
    sphere_irreg_waves_eta_test
    PRIVATE
        ${SPHERE_IRREG_WAVES_ETA_SRC_DIR}/sphere_irreg_waves_eta_test.cpp
)

setup_test_target(sphere_irreg_waves_eta_test ${SPHERE_IRREG_WAVES_ETA_SRC_DIR} ${SPHERE_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(sphere_irreg_waves_eta_test PROPERTIES FOLDER "regression/sphere")

# Register as CTest test
if(TARGET sphere_irreg_waves_eta_test)
    # Copy test data to the output directory (only once at top level)
    # copy_test_data(${SPHERE_SRC_DIR} ${SPHERE_RELEASE_DIR})
    
    add_test(
        NAME sphere_irreg_waves_eta_regression
        COMMAND ${SPHERE_RELEASE_DIR}/sphere_irreg_waves_eta_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        sphere_irreg_waves_eta_regression
        PROPERTIES 
        LABELS "regression;sphere;irreg_waves_eta;core"
        FIXTURES_SETUP sphere_irreg_waves_eta_regression_file
        WORKING_DIRECTORY ${SPHERE_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test
    add_test(
        NAME sphere_irreg_waves_eta_regression_ref
        COMMAND python ${CMAKE_SOURCE_DIR}/tests/regression/sphere/irreg_waves_eta/compare.py
    )
    
    set_tests_properties(
        sphere_irreg_waves_eta_regression_ref
        PROPERTIES 
        LABELS "regression;sphere;irreg_waves_eta;reference;core"
        FIXTURES_REQUIRED sphere_irreg_waves_eta_regression_file
        WORKING_DIRECTORY ${SPHERE_RELEASE_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# OSWEC Decay Regression Test
# ===========================

# Create executable in model-based directory
add_executable(oswec_decay_test)

set(OSWEC_DECAY_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/oswec)
set(OSWEC_DECAY_RELEASE_DIR ${TEST_REGRESSION_OUTPUT_DIR}/oswec)
set(OSWEC_DECAY_RESULTS_DIR ${OSWEC_DECAY_RELEASE_DIR}/results)

target_sources(
    oswec_decay_test
    PRIVATE
        ${OSWEC_DECAY_SRC_DIR}/oswec_decay_test.cpp
)

setup_test_target(oswec_decay_test ${OSWEC_DECAY_SRC_DIR} ${OSWEC_DECAY_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(oswec_decay_test PROPERTIES FOLDER "regression/oswec")

# Register as CTest test
if(TARGET oswec_decay_test)
    # Copy test data to the output directory
    copy_test_data(${OSWEC_DECAY_SRC_DIR} ${OSWEC_DECAY_RELEASE_DIR})
    
    add_test(
        NAME oswec_decay_regression
        COMMAND ${OSWEC_DECAY_RELEASE_DIR}/oswec_decay_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        oswec_decay_regression
        PROPERTIES 
        LABELS "regression;oswec;decay;core"
        FIXTURES_SETUP oswec_decay_regression_file
        WORKING_DIRECTORY ${OSWEC_DECAY_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test
    set(FILE_RST ${OSWEC_DECAY_RESULTS_DIR}/CHRONO_OSWEC_DECAY.txt)
    set(FILE_REF ${CMAKE_SOURCE_DIR}/tests/regression/reference_data/oswec/decay/hc_ref_oswec_decay.txt)

    add_test(
        NAME oswec_decay_regression_ref
        COMMAND python compare_decay.py ${FILE_REF} ${FILE_RST}
    )
    
    set_tests_properties(
        oswec_decay_regression_ref
        PROPERTIES 
        LABELS "regression;oswec;decay;reference;core"
        FIXTURES_REQUIRED oswec_decay_regression_file
        WORKING_DIRECTORY ${OSWEC_DECAY_SRC_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# OSWEC Regular Waves Regression Test
# ===================================

# Create executable in model-based directory
add_executable(oswec_reg_waves_test)

set(OSWEC_REG_WAVES_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/oswec)
set(OSWEC_REG_WAVES_RELEASE_DIR ${TEST_REGRESSION_OUTPUT_DIR}/oswec)
set(OSWEC_REG_WAVES_RESULTS_DIR ${OSWEC_REG_WAVES_RELEASE_DIR}/results)

target_sources(
    oswec_reg_waves_test
    PRIVATE
        ${OSWEC_REG_WAVES_SRC_DIR}/oswec_reg_waves_test.cpp
)

setup_test_target(oswec_reg_waves_test ${OSWEC_REG_WAVES_SRC_DIR} ${OSWEC_REG_WAVES_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(oswec_reg_waves_test PROPERTIES FOLDER "regression/oswec")

# Register as CTest test
if(TARGET oswec_reg_waves_test)
    # Copy test data to the output directory
    copy_test_data(${OSWEC_REG_WAVES_SRC_DIR} ${OSWEC_REG_WAVES_RELEASE_DIR})
    
    add_test(
        NAME oswec_reg_waves_regression
        COMMAND ${OSWEC_REG_WAVES_RELEASE_DIR}/oswec_reg_waves_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        oswec_reg_waves_regression
        PROPERTIES 
        LABELS "regression;oswec;reg_waves;core"
        FIXTURES_SETUP oswec_reg_waves_regression_file
        WORKING_DIRECTORY ${OSWEC_REG_WAVES_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test - for now, just test wave condition 1
    set(FILE_RST ${OSWEC_REG_WAVES_RESULTS_DIR}/CHRONO_OSWEC_REG_WAVES_1.txt)
    set(FILE_REF ${CMAKE_SOURCE_DIR}/tests/regression/reference_data/oswec/reg_waves/hc_ref_oswec_reg_waves_1.txt)

    add_test(
        NAME oswec_reg_waves_regression_ref
        COMMAND python compare_reg_waves.py ${FILE_REF} ${FILE_RST}
    )
    
    set_tests_properties(
        oswec_reg_waves_regression_ref
        PROPERTIES 
        LABELS "regression;oswec;reg_waves;reference;core"
        FIXTURES_REQUIRED oswec_reg_waves_regression_file
        WORKING_DIRECTORY ${OSWEC_REG_WAVES_SRC_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# RM3 Decay Regression Test
# =========================

# Create executable in model-based directory
add_executable(rm3_decay_test)

set(RM3_DECAY_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rm3)
set(RM3_DECAY_RELEASE_DIR ${TEST_REGRESSION_OUTPUT_DIR}/rm3)
set(RM3_DECAY_RESULTS_DIR ${RM3_DECAY_RELEASE_DIR}/results)

target_sources(
    rm3_decay_test
    PRIVATE
        ${RM3_DECAY_SRC_DIR}/rm3_decay_test.cpp
)

setup_test_target(rm3_decay_test ${RM3_DECAY_SRC_DIR} ${RM3_DECAY_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(rm3_decay_test PROPERTIES FOLDER "regression/rm3")

# Register as CTest test
if(TARGET rm3_decay_test)
    # Copy test data to the output directory
    copy_test_data(${RM3_DECAY_SRC_DIR} ${RM3_DECAY_RELEASE_DIR})
    
    add_test(
        NAME rm3_decay_regression
        COMMAND ${RM3_DECAY_RELEASE_DIR}/rm3_decay_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        rm3_decay_regression
        PROPERTIES 
        LABELS "regression;rm3;decay;core"
        FIXTURES_SETUP rm3_decay_regression_file
        WORKING_DIRECTORY ${RM3_DECAY_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test
    set(FILE_RST ${RM3_DECAY_RESULTS_DIR}/CHRONO_RM3_DECAY.txt)
    set(FILE_REF ${CMAKE_SOURCE_DIR}/tests/regression/reference_data/rm3/decay/hc_ref_rm3_decay.txt)

    add_test(
        NAME rm3_decay_regression_ref
        COMMAND python compare_decay.py ${FILE_REF} ${FILE_RST}
    )
    
    set_tests_properties(
        rm3_decay_regression_ref
        PROPERTIES 
        LABELS "regression;rm3;decay;reference;core"
        FIXTURES_REQUIRED rm3_decay_regression_file
        WORKING_DIRECTORY ${RM3_DECAY_SRC_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# RM3 Regular Waves Regression Test
# =================================

# Create executable in model-based directory
add_executable(rm3_reg_waves_test)

set(RM3_REG_WAVES_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rm3)
set(RM3_REG_WAVES_RELEASE_DIR ${TEST_REGRESSION_OUTPUT_DIR}/rm3)
set(RM3_REG_WAVES_RESULTS_DIR ${RM3_REG_WAVES_RELEASE_DIR}/results)

target_sources(
    rm3_reg_waves_test
    PRIVATE
        ${RM3_REG_WAVES_SRC_DIR}/rm3_reg_waves_test.cpp
)

setup_test_target(rm3_reg_waves_test ${RM3_REG_WAVES_SRC_DIR} ${RM3_REG_WAVES_RELEASE_DIR})

# Organize in Visual Studio solution
set_target_properties(rm3_reg_waves_test PROPERTIES FOLDER "regression/rm3")

# Register as CTest test
if(TARGET rm3_reg_waves_test)
    # Copy test data to the output directory
    copy_test_data(${RM3_REG_WAVES_SRC_DIR} ${RM3_REG_WAVES_RELEASE_DIR})
    
    add_test(
        NAME rm3_reg_waves_regression
        COMMAND ${RM3_REG_WAVES_RELEASE_DIR}/rm3_reg_waves_test.exe ${HYDROCHRONO_DATA_DIR} --nogui
    )
    
    set_tests_properties(
        rm3_reg_waves_regression
        PROPERTIES 
        LABELS "regression;rm3;reg_waves;core"
        FIXTURES_SETUP rm3_reg_waves_regression_file
        WORKING_DIRECTORY ${RM3_REG_WAVES_RELEASE_DIR}
        ENVIRONMENT "${TEST_ENVIRONMENT}"
    )

    # Reference comparison test
    set(FILE_RST ${RM3_REG_WAVES_RESULTS_DIR}/CHRONO_RM3_REG_WAVES.txt)
    set(FILE_REF ${CMAKE_SOURCE_DIR}/tests/regression/reference_data/rm3/reg_waves/hc_ref_rm3_reg_waves.txt)

    add_test(
        NAME rm3_reg_waves_regression_ref
        COMMAND python compare_reg_waves.py ${FILE_REF} ${FILE_RST}
    )
    
    set_tests_properties(
        rm3_reg_waves_regression_ref
        PROPERTIES 
        LABELS "regression;rm3;reg_waves;reference;core"
        FIXTURES_REQUIRED rm3_reg_waves_regression_file
        WORKING_DIRECTORY ${RM3_REG_WAVES_SRC_DIR}
        ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    )
endif() 

# Regression Test Report Generation
# =================================

# Create documentation directory next to other regression artefacts
set(REGRESSION_DOCS_DIR ${TEST_REGRESSION_OUTPUT_DIR}/report)
file(MAKE_DIRECTORY ${REGRESSION_DOCS_DIR})

# Add a test that generates the regression report (markdown + PDF) after all tests complete
add_test(
    NAME regression_report_generation
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/regression/utilities/generate_report.py --build-dir ${CMAKE_BINARY_DIR} --output-dir ${REGRESSION_DOCS_DIR} --pdf
)

# Make sure report generation runs after all other regression tests
set_tests_properties(
    regression_report_generation
    PROPERTIES 
    LABELS "regression;report;documentation;core"
    WORKING_DIRECTORY ${TEST_REGRESSION_OUTPUT_DIR}
    ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    DEPENDS "sphere_decay_regression;sphere_reg_waves_regression;sphere_irreg_waves_regression;sphere_irreg_waves_eta_regression;f3of_dt1_regression;f3of_dt2_regression;f3of_dt3_regression;oswec_decay_regression;oswec_reg_waves_regression;rm3_decay_regression;rm3_reg_waves_regression"
)