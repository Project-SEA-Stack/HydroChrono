add_executable(test_hydro_yaml_parser test_hydro_yaml_parser.cpp)
source_group("" FILES "test_hydro_yaml_parser.cpp")

#target_include_directories(test_hydro_yaml_parser 
#    PRIVATE 
#    ../../src
#    ../../include
#)

if(MSVC)
   set_target_properties(${TEST_EXE} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TEST_EXE}>")
endif()

target_link_libraries(test_hydro_yaml_parser PRIVATE HydroChrono)

#target_link_libraries(test_hydro_yaml_parser 
#    PRIVATE 
#    Chrono::Chrono_core
#)

# Register as CTest
add_test(NAME test_hydro_yaml_parser COMMAND test_hydro_yaml_parser)
set_tests_properties(test_hydro_yaml_parser 
    PROPERTIES 
    LABELS "unit"
    ENVIRONMENT "${TEST_ENVIRONMENT}"
)
