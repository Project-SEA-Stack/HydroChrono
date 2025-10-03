# Unit tests for HydroChrono
# =========================

# Hydro YAML Parser Unit Test
add_executable(test_hydro_yaml_parser
    test_hydro_yaml_parser.cpp
    ../../src/hydro_yaml_parser.cpp
)

target_include_directories(test_hydro_yaml_parser 
    PRIVATE 
    ../../src
    ../../include
)

target_link_libraries(test_hydro_yaml_parser 
    PRIVATE 
    Chrono::Chrono_core
)

# Register as CTest
add_test(NAME test_hydro_yaml_parser COMMAND test_hydro_yaml_parser)
set_tests_properties(test_hydro_yaml_parser 
    PROPERTIES 
    LABELS "unit"
    ENVIRONMENT "${TEST_ENVIRONMENT}"
)