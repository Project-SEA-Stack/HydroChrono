function(build_demo DEMO_GROUP DEMO_NAME)

    message(STATUS "...${DEMO_NAME}")

    set(DEMO_SRC ${DEMO_GROUP}/demo_${DEMO_NAME}.cpp)
    set(DEMO_EXE demo_${DEMO_NAME})

    add_executable(${DEMO_EXE} ${DEMO_SRC})
    source_group("" FILES ${DEMO_SRC})
    
    target_compile_definitions(${DEMO_EXE} 
         PRIVATE
        "RESULTS_DIR_NAME=\"${DEMO_GROUP}\""
    )

    if(MSVC)
        set_target_properties(${DEMO_EXE} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${DEMO_EXE}>")
    endif()

    target_link_libraries(${DEMO_EXE} PRIVATE HydroChronoGUI HydroChrono)

    if(HC_INSTALL_DEV_DEMOS)
        install(TARGETS ${DEMO_EXE}  CONFIGURATIONS Release DESTINATION bin COMPONENT runtime)
    endif()

endfunction()

# Build demos
# ===========

message(STATUS "Add HydroChrono demos")

build_demo(sphere    sphere_decay)
build_demo(sphere    sphere_reg_waves)
build_demo(sphere    sphere_irreg_waves)
build_demo(sphere    sphere_irreg_waves_eta_import)
build_demo(rm3       rm3_decay)
build_demo(rm3       rm3_reg_waves)
build_demo(oswec     oswec_decay)
build_demo(oswec     oswec_reg_waves)
build_demo(f3of      f3of_DT1)
build_demo(f3of      f3of_DT2)
build_demo(f3of      f3of_DT3)
build_demo(DeepCWind DeepCWind_decay)
