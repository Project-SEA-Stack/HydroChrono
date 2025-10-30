message(STATUS "Add run_hydrochrono program")

add_executable(run_hydrochrono
    run_hydrochrono.cpp
    ../src/hydrochrono_runner/run_hydrochrono_from_yaml.cpp
    ../src/utils/misc_options.cpp
    ../src/utils/setup_parser.cpp
)

# Set C++ standard
target_compile_features(run_hydrochrono PUBLIC ${HC_CXX_STANDARD})

target_link_libraries(run_hydrochrono
    PRIVATE HydroChrono HydroChronoGUI Chrono::Chrono_parsers
)

# Safety net: if HDF5 was not propagated, link executable explicitly as well
if(HC_HDF5_TARGETS)
    target_link_libraries(run_hydrochrono PRIVATE ${HC_HDF5_TARGETS})
elseif(DEFINED HDF5_CXX_LIBRARIES OR DEFINED HDF5_LIBRARIES)
    target_link_libraries(run_hydrochrono PRIVATE ${HDF5_CXX_LIBRARIES} ${HDF5_LIBRARIES})
endif()

target_include_directories(run_hydrochrono
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/utils
        # Fallback include paths for Chrono Parsers when using a build-tree Chrono_DIR
        $<IF:$<BOOL:${Chrono_DIR}>,${Chrono_DIR}/../../install/include,>
        $<IF:$<BOOL:${Chrono_DIR}>,${Chrono_DIR}/../include,>
        $<IF:$<BOOL:${Chrono_DIR}>,${Chrono_DIR}/../../src,>
)

if(MSVC)
    set_target_properties(run_hydrochrono PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:run_hydrochrono>")
endif()

set_target_properties(run_hydrochrono PROPERTIES
    OUTPUT_NAME "run_hydrochrono"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
