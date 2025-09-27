# # =====================
# # SPHERE_DECAY
# # =====================
# add_executable(demo_sphere_decay)
# 
# target_sources(
#     demo_sphere_decay
# 
#     PRIVATE
#         sphere/demo_sphere_decay.cpp
# )
# 
# if(HYDROCHRONO_ENABLE_IRRLICHT)
# 	target_compile_definitions(demo_sphere_decay 
# 		PRIVATE 
# 		HYDROCHRONO_HAVE_IRRLICHT=1
# 		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
# 	)
# endif()
# 
# target_include_directories(
# 	demo_sphere_decay
# 
# 	PRIVATE
# 	    ${CMAKE_CURRENT_SOURCE_DIR}/
# 	    ${CMAKE_CURRENT_SOURCE_DIR}/include
# )
# 
# target_link_libraries(demo_sphere_decay 
# 	PRIVATE
# 	HydroChronoGUI
# 	HydroChrono
# 	
# )

# =====================
# SPHERE_REG_WAVES
# =====================
add_executable(demo_sphere_reg_waves)

target_sources(
    demo_sphere_reg_waves

    PRIVATE
        sphere/demo_sphere_reg_waves.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_sphere_reg_waves 
		PRIVATE 
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_sphere_reg_waves

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(demo_sphere_reg_waves 
    PRIVATE 
	HydroChronoGUI	
    HydroChrono
)


# =====================
# SPHERE_IRREG_WAVES

# =====================
add_executable(demo_sphere_irreg_waves)

target_sources(
    demo_sphere_irreg_waves

    PRIVATE
        sphere/demo_sphere_irreg_waves.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_sphere_irreg_waves 
		PRIVATE 
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_sphere_irreg_waves

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(demo_sphere_irreg_waves 
    PRIVATE 
    HydroChronoGUI
    HydroChrono
)

# =====================
# SPHERE_IRREG_WAVES_ETA_IMPORT

# =====================
add_executable(demo_sphere_irreg_waves_eta_import)

target_sources(
    demo_sphere_irreg_waves_eta_import

    PRIVATE
        sphere/demo_sphere_irreg_waves_eta_import.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_sphere_irreg_waves_eta_import 
		PRIVATE 
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_sphere_irreg_waves_eta_import

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(demo_sphere_irreg_waves_eta_import 
    PRIVATE 
    HydroChronoGUI
    HydroChrono
)

# =====================
# RM3_DECAY
# =====================
add_executable(demo_rm3_decay)

target_sources(
    demo_rm3_decay

    PRIVATE
    rm3/demo_rm3_decay.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_rm3_decay
		PRIVATE 
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_rm3_decay

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(demo_rm3_decay
    PRIVATE 
	HydroChronoGUI	
    HydroChrono
)


# =====================
# RM3_REG_WAVES
# =====================
add_executable(demo_rm3_reg_waves)

target_sources(
    demo_rm3_reg_waves

    PRIVATE
    rm3/demo_rm3_reg_waves.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_rm3_reg_waves
		PRIVATE 
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_rm3_reg_waves

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(demo_rm3_reg_waves 
    PRIVATE
	HydroChronoGUI	
    HydroChrono
)


# =====================
# OSWEC_DECAY
# =====================
add_executable(demo_oswec_decay)

target_sources(
    demo_oswec_decay

    PRIVATE
    oswec/demo_oswec_decay.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_oswec_decay
		PRIVATE 
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_oswec_decay

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(demo_oswec_decay
    PRIVATE
	HydroChronoGUI
    HydroChrono
)

# =====================
# OSWEC_REG_WAVES
# =====================
add_executable(demo_oswec_reg_waves)

target_sources(
    demo_oswec_reg_waves

    PRIVATE
    oswec/demo_oswec_reg_waves.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_oswec_reg_waves
		PRIVATE 
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_oswec_reg_waves

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(demo_oswec_reg_waves
    PRIVATE
	HydroChronoGUI
    HydroChrono
)

# =====================
# F3OF DT 1
# =====================
add_executable(demo_f3of_DT1)

target_sources(
    demo_f3of_DT1

    PRIVATE
    f3of/demo_F3OF_DT1.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_f3of_DT1
		PRIVATE
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_f3of_DT1

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(demo_f3of_DT1
    PRIVATE
	HydroChronoGUI	
    HydroChrono
)

# =====================
# F3OF DT 2
# =====================
add_executable(demo_f3of_DT2)

target_sources(
    demo_f3of_DT2

    PRIVATE
    f3of/demo_F3OF_DT2.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_f3of_DT2
		PRIVATE
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_f3of_DT2

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(demo_f3of_DT2
    PRIVATE
	HydroChronoGUI	
    HydroChrono
)
# =====================
# F3OF DT 3
# =====================
add_executable(demo_f3of_DT3)

target_sources(
    demo_f3of_DT3

    PRIVATE
    f3of/demo_F3OF_DT3.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_f3of_DT3
		PRIVATE
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_f3of_DT3

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(demo_f3of_DT3
    PRIVATE
	HydroChronoGUI	
    HydroChrono
)

# =====================
# DeepCWind
# =====================
add_executable(demo_DeepCWind_decay)

target_sources(
    demo_DeepCWind_decay

    PRIVATE
    DeepCWind/demo_DeepCWind_decay.cpp
)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	target_compile_definitions(demo_DeepCWind_decay
		PRIVATE
		HYDROCHRONO_HAVE_IRRLICHT=1
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
	)
endif()

target_include_directories(
	demo_DeepCWind_decay

	PRIVATE
	    ${CMAKE_CURRENT_SOURCE_DIR}/
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(demo_DeepCWind_decay
    PRIVATE
    HydroChronoGUI
    HydroChrono
)