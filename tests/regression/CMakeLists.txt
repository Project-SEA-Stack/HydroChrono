# TODO
#  - properly set WORKING_DIRECTORY (different is using multi-config or single-config generators)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

function(build_regression_test TEST_GROUP TEST_NAME TEST_REF_FILE)

   message(STATUS "...${TEST_NAME}")
   
   set(TEST_SRC ${TEST_GROUP}/test_${TEST_NAME}.cpp)
   set(TEST_EXE test_${TEST_NAME})
   set(TEST_REF ${TEST_NAME}_regression)
   set(COMPARE_SCRIPT ${CMAKE_SOURCE_DIR}/tests/regression/${TEST_GROUP}/compare_${TEST_NAME}.py)
   set(FIXTURE_FILE ${TEST_NAME}_regression_file)
   set(FILE_RST ${CMAKE_BINARY_DIR}/bin/test_results/${TEST_GROUP}/results_${TEST_NAME}.txt)
   set(FILE_REF ${CMAKE_SOURCE_DIR}/data/reference_data/${TEST_GROUP}/${TEST_REF_FILE})
   
   add_executable(${TEST_EXE} ${TEST_SRC})
   source_group("" FILES ${TEST_SRC})

   target_link_libraries(${TEST_EXE} PRIVATE HydroChrono HydroChronoGUI)

   add_test(
       NAME ${TEST_EXE}
       COMMAND ${PROJECT_BINARY_DIR}/bin/${TEST_EXE} --nogui)

   set_tests_properties(
       ${TEST_EXE}
       PROPERTIES 
       LABELS "regression;${TARGET_GROUP};core"
       FIXTURES_SETUP ${FIXTURE_FILE}
       #WORKING_DIRECTORY ${TEST_REGRESSION_OUTPUT_DIR}/${TEST_GROUP}
       ENVIRONMENT "${TEST_ENVIRONMENT}"
   )

   # Reference comparison test - use the new centralized reference data location

   add_test(
       NAME ${TEST_REF}
       COMMAND ${Python3_EXECUTABLE} ${COMPARE_SCRIPT} ${FILE_REF} ${FILE_RST}
   )
   
   set_tests_properties(
       ${TEST_REF}
       PROPERTIES 
       LABELS "regression;${TARGET_GROUP};reference;core"
       FIXTURES_REQUIRED ${FIXTURE_FILE}
       #WORKING_DIRECTORY ${TEST_REGRESSION_OUTPUT_DIR}/${TEST_GROUP}
       ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
   )

endfunction()

# Build tests
# ===========

message(STATUS "Add HydroChrono regression tests")

build_regression_test(sphere   sphere_decay               hc_ref_sphere_decay.txt)
build_regression_test(sphere   sphere_reg_waves           hc_ref_sphere_reg_waves_1.txt)
build_regression_test(sphere   sphere_irreg_waves         hc_ref_sphere_irreg_waves.txt)
build_regression_test(sphere   sphere_irreg_waves_eta     hc_ref_sphere_irreg_waves_eta.txt)
build_regression_test(f3of     f3of_dt1                   hc_ref_f3of_dt1_surge.txt)
build_regression_test(f3of     f3of_dt2                   hc_ref_f3of_dt2_pitch.txt)
build_regression_test(f3of     f3of_dt3                   hc_ref_f3of_dt3_flap_pitch.txt)
build_regression_test(oswec    oswec_decay                hc_ref_oswec_decay.txt)
build_regression_test(oswec    oswec_reg_waves            hc_ref_oswec_reg_waves_1.txt)
build_regression_test(rm3      rm3_decay                  hc_ref_rm3_decay.txt)
build_regression_test(rm3      rm3_reg_waves              hc_ref_rm3_reg_waves.txt)

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
#    WORKING_DIRECTORY ${TEST_REGRESSION_OUTPUT_DIR}
    ENVIRONMENT "PATH=${Python3_ROOT_DIR};$ENV{PATH};HYDROCHRONO_BUILD_DIR=${CMAKE_BINARY_DIR}"
    DEPENDS "sphere_decay_regression;sphere_reg_waves_regression;sphere_irreg_waves_regression;sphere_irreg_waves_eta_regression;f3of_dt1_regression;f3of_dt2_regression;f3of_dt3_regression;oswec_decay_regression;oswec_reg_waves_regression;rm3_decay_regression;rm3_reg_waves_regression"
)
