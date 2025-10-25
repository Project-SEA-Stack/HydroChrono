set(DEMOS
   #sphere/demo_sphere_decay
   sphere/demo_sphere_reg_waves
   sphere/demo_sphere_irreg_waves
   sphere/demo_sphere_irreg_waves_eta_import
   rm3/demo_rm3_decay
   rm3/demo_rm3_reg_waves
   oswec/demo_oswec_decay
   oswec/demo_oswec_reg_waves
   f3of/demo_f3of_DT1
   f3of/demo_f3of_DT2
   f3of/demo_f3of_DT3
   DeepCWind/demo_DeepCWind_decay
)

message(STATUS "Add HydroChrono demos")

foreach(DEMO ${DEMOS})

   get_filename_component(PROGRAM ${DEMO} NAME)
   message(STATUS "...${PROGRAM}")
   
   add_executable(${PROGRAM} ${DEMO}.cpp)
   source_group("" FILES ${DEMO}.cpp)
   
   if(MSVC)
     set_target_properties(${PROGRAM} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROGRAM}>")
   endif()

   target_link_libraries(${PROGRAM} PRIVATE HydroChronoGUI HydroChrono)

endforeach()
