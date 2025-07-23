# ═══════════════════════════════════════════════════════════════════════════════
# ─── 1. Prerequisites & Policies ───────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.18.2)

# CMP0091: Enable MSVC runtime library selection via CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0091 NEW)

# Guard against in-source builds to prevent polluting source directory
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt.")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 2. Project Definition & Defaults ─────────────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

project(HydroChrono 
	VERSION 0.2.6
	DESCRIPTION "A hydrodynamic library for use with Project Chrono."
	LANGUAGES CXX
)

# Add the cmake folder so the FindSphinx module is found
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Force Release by default so CI builds are optimized and users get good performance
if(NOT DEFINED HYDROCHRONO_DEFAULT_BUILD_TYPE)
	set(HYDROCHRONO_DEFAULT_BUILD_TYPE "Release")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${HYDROCHRONO_DEFAULT_BUILD_TYPE}' as none was specified.")
	set(CMAKE_BUILD_TYPE ${HYDROCHRONO_DEFAULT_BUILD_TYPE}
	    CACHE STRING "Choose the type of build." FORCE)
	mark_as_advanced(CMAKE_BUILD_TYPE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 3. User Options ───────────────────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

option(HYDROCHRONO_ENABLE_TESTS "Enable tests" ON)
option(HYDROCHRONO_ENABLE_IRRLICHT "Enable irrlicht visualization library" ON)
option(HYDROCHRONO_ENABLE_DEMOS "Enable demo executables" ON)
option(HYDROCHRONO_ENABLE_USER_DOC "User's documentation" OFF)
option(HYDROCHRONO_ENABLE_PROG_DOC "Programmer's documentation" OFF)
option(HYDROCHRONO_ENABLE_LOGGING "Enable debug logging" OFF)

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 4. Find Dependencies ─────────────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════────────────────────

# ── Chrono Integration ──
# Important! To ensure ABI compatibility, use the same C++ standard
# as the one used to build the Chrono libraries.
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD ${CHRONO_CXX_STANDARD})

# If using Irrlicht, require Chrono built with Irrlicht support
if(HYDROCHRONO_ENABLE_IRRLICHT)
	find_package(Chrono COMPONENTS Irrlicht CONFIG REQUIRED)
	add_compile_definitions(HYDROCHRONO_HAVE_IRRLICHT=1)
else()
	find_package(Chrono CONFIG REQUIRED)
endif()

# Ensure we have a compatible Chrono version
if(Chrono_VERSION VERSION_LESS "9.0.0")
	message(WARNING "HydroChrono has been tested with Chrono ≥ 9.0.0. Found version: ${Chrono_VERSION}")
endif()

# ── HDF5 Integration ──
# Force static linking to avoid DLL deployment issues
set(HDF5_USE_STATIC_LIBRARIES ON)
set(HDF5_ROOT "${HDF5_ROOT}" CACHE PATH "HDF5 root directory")
find_package(HDF5 REQUIRED COMPONENTS CXX)

# ── Platform-Specific Fixes ──
# Fix for VS 2017 15.8+ to handle alignment specification with Eigen
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	if(MSVC AND ${MSVC_VERSION} GREATER_EQUAL 1915)
		add_compile_definitions("ENABLE_EXTENDED_ALIGNED_STORAGE")
	endif()
endif()

# Disable common MSVC warnings that pollute build output
if(MSVC)
	add_definitions(
		-D_CRT_SECURE_NO_DEPRECATE   # Disable CRT deprecation warnings
		-D_SCL_SECURE_NO_DEPRECATE   # Disable SCL deprecation warnings
		-DNOMINMAX                   # Prevent Windows.h from defining min/max macros
	)
	add_compile_options(/wd4275)     # Disable warnings triggered by Irrlicht
	add_compile_options(/wd4251)     # Disable "class needs to have dll-interface" warnings
endif()

# ── Output Directory Structure ──
# Centralize all binaries for easier deployment and testing
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Replicate for multi-config generators (Visual Studio, Xcode, Ninja Multi-Config)
if(CMAKE_CONFIGURATION_TYPES)
	foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
		string(TOUPPER "${cfg}" cfg_uc)
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_uc} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_uc} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
		set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_uc} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
	endforeach()
endif()

# Category-specific output folders for organization
set(HYDROCHRONO_DEMO_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/demos CACHE PATH "Output directory for demo executables")
set(HYDROCHRONO_TEST_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests CACHE PATH "Output directory for test executables")

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 5. Core Library Target ───────────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

# Helper function to configure common HydroChrono target properties
function(configure_hydro_target tgt sources)
	add_library(${tgt} ${sources})
	
	target_compile_features(${tgt} PUBLIC cxx_std_17)
	
	target_include_directories(${tgt}
		PUBLIC
			$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
			$<INSTALL_INTERFACE:include>
			${CHRONO_INCLUDE_DIRS}
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/
			${CMAKE_CURRENT_SOURCE_DIR}/include
	)
	
	# Inherit Chrono's compiler flags for ABI compatibility
	target_compile_options(${tgt} BEFORE PUBLIC ${CHRONO_CXX_FLAGS})
	target_link_options(${tgt} BEFORE PUBLIC ${CHRONO_LINKER_FLAGS})
	
	# Enable position-independent code for static library linking
	set_target_properties(${tgt} PROPERTIES POSITION_INDEPENDENT_CODE ON)
	
	# Link to Chrono libraries for public API compatibility
	target_link_libraries(${tgt} PUBLIC ${CHRONO_LIBRARIES})
endfunction()

# ── Main HydroChrono Library ──
set(HYDROCHRONO_SOURCES
	src/h5fileinfo.cpp
	src/chloadaddedmass.cpp
	src/hydro_forces.cpp
	src/helper.cpp
	src/wave_types.cpp
	src/logging.cpp
)

configure_hydro_target(HydroChrono "${HYDROCHRONO_SOURCES}")

# Add HDF5 as private dependency (don't expose to users)
target_include_directories(HydroChrono PRIVATE ${HDF5_INCLUDE_DIRS})
target_link_libraries(HydroChrono PRIVATE ${HDF5_LIBRARIES})

# Library-specific compile definitions
target_compile_definitions(HydroChrono
	PUBLIC
		CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\"
		HYDROCHRONO_VERSION=\"${PROJECT_VERSION}\"
		HYDROCHRONO_BUILD_TYPE=\"$<IF:$<CONFIG:Debug>,Debug,$<IF:$<CONFIG:Release>,Release,$<IF:$<CONFIG:RelWithDebInfo>,RelWithDebInfo,$<IF:$<CONFIG:MinSizeRel>,MinSizeRel,Unknown>>>>\"
	PRIVATE 
		$<$<BOOL:${HYDROCHRONO_ENABLE_LOGGING}>:HYDROCHRONO_ENABLE_LOGGING=1>
)

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 6. Auxiliary Targets (GUI, Tests, Demos) ─────────────────────────────────
# ═══════════════════════════════════════════════════────────════────────────════

# ── GUI Helper Library ──
# Separate library for Irrlicht visualization helpers
configure_hydro_target(HydroChronoGUI "src/gui/guihelper.cpp")

# ── Demo Executables ──
if(HYDROCHRONO_ENABLE_DEMOS)
	add_subdirectory(demos)
endif()

# ── Test Suite ──
if(HYDROCHRONO_ENABLE_TESTS)
	# Use environment variable to override default data directory
	if(DEFINED ENV{HYDROCHRONO_DATA_DIR})
		set(HYDROCHRONO_DATA_DIR $ENV{HYDROCHRONO_DATA_DIR})
	else()
		set(HYDROCHRONO_DATA_DIR "${PROJECT_SOURCE_DIR}/demos")
	endif()

	include(CTest)
	enable_testing()
	
	# Set up DLL search paths for Windows testing
	set(CHRONO_DLL_DIR "${Chrono_DIR}/../bin/Release")
	set(IRRLICHT_DLL_DIR "C:/libs/irrlicht-1.8.4/bin/Win64-VisualStudio")
	set(TEST_ENVIRONMENT "PATH=${CHRONO_DLL_DIR};${IRRLICHT_DLL_DIR};$ENV{PATH}")

	add_subdirectory(tests)
	add_subdirectory(tests/regression)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 7. Installation ───────────────────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

include(GNUInstallDirs)

# Export HydroChrono as an importable target for other CMake projects
install(TARGETS HydroChrono
	EXPORT HydroChronoTargets
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT HydroChronoTargets
	FILE HydroChronoTargets.cmake
	DESTINATION "${CMAKE_INSTALL_DATADIR}/HydroChrono/cmake"
	NAMESPACE HydroChrono::
)

# Install public headers
install(DIRECTORY include/hydroc
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Generate and install package config files for find_package() support
include(CMakePackageConfigHelpers)
configure_package_config_file(
	cmake/HydroChronoConfig.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/HydroChronoConfig.cmake"
	INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/HydroChrono/cmake"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/HydroChronoConfig.cmake"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/HydroChrono/cmake"
)

# Export targets for build tree usage (without installation)
export(EXPORT HydroChronoTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/HydroChronoTargets.cmake"
	NAMESPACE HydroChrono::
)

# ── Debug Symbols in Release Mode ──
# Keep debug symbols in Release builds for better profiling and crash analysis
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi /O2")
