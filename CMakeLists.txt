# ═══════════════════════════════════════════════════════════════════════════════
# ─── 1. Prerequisites & Policies ───────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

# cmake_minimum_required(3.18.2): need CMP0091 policy + target properties support
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

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 2.1. Parse Project Metadata ──────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

# Read and parse project.meta file
file(READ "${CMAKE_SOURCE_DIR}/project.meta" PROJECT_META_CONTENT)
string(REPLACE "\n" ";" PROJECT_META_LINES "${PROJECT_META_CONTENT}")

# Parse each line and extract key-value pairs
foreach(line ${PROJECT_META_LINES})
    if(line MATCHES "^([^=]+)=([^=]+)$")
        string(STRIP "${CMAKE_MATCH_1}" key)
        string(STRIP "${CMAKE_MATCH_2}" value)
        if(key STREQUAL "name")
            set(HYDROCHRONO_NAME "${value}")
        elseif(key STREQUAL "version")
            set(HYDROCHRONO_VERSION "${value}")
        elseif(key STREQUAL "description")
            set(HYDROCHRONO_DESCRIPTION "${value}")
        elseif(key STREQUAL "author")
            set(HYDROCHRONO_AUTHOR "${value}")
        elseif(key STREQUAL "maintainer")
            set(HYDROCHRONO_MAINTAINER "${value}")
        elseif(key STREQUAL "url")
            set(HYDROCHRONO_URL "${value}")
        elseif(key STREQUAL "license")
            set(HYDROCHRONO_LICENSE "${value}")
        elseif(key STREQUAL "status")
            set(HYDROCHRONO_STATUS "${value}")
        endif()
    endif()
endforeach()

# Display parsed metadata
message(STATUS "Project: ${HYDROCHRONO_NAME} v${HYDROCHRONO_VERSION}")
message(STATUS "Description: ${HYDROCHRONO_DESCRIPTION}")
message(STATUS "Author: ${HYDROCHRONO_AUTHOR}")
message(STATUS "Maintainer: ${HYDROCHRONO_MAINTAINER}")
message(STATUS "License: ${HYDROCHRONO_LICENSE}")
message(STATUS "Status: ${HYDROCHRONO_STATUS}")

# Generate version.h header from template
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in
               ${CMAKE_BINARY_DIR}/generated/hydroc/version.h @ONLY)

# Add generated include directory
include_directories(${CMAKE_BINARY_DIR}/generated)

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
option(HYDROCHRONO_ENABLE_YAML_RUNNER "Enable YAML-based CLI runner" ON)
option(HYDROCHRONO_ENABLE_USER_DOC "User's documentation" OFF)
option(HYDROCHRONO_ENABLE_PROG_DOC "Programmer's documentation" OFF)
option(HYDROCHRONO_ENABLE_LOGGING "Enable debug logging" OFF)

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 4. Find Dependencies ─────────────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

# ── Chrono Integration ──
# Important! To ensure ABI compatibility, use the same C++ standard
# as the one used to build the Chrono libraries.
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD ${CHRONO_CXX_STANDARD})

# Add Chrono's cmake path so its custom FindIrrlicht module is discoverable
if(DEFINED Chrono_DIR)
	list(APPEND CMAKE_MODULE_PATH "${Chrono_DIR}")
endif()

# Optionally allow the user to set Irrlicht_ROOT externally (e.g., via build.ps1)
set(Irrlicht_ROOT "${Irrlicht_ROOT}" CACHE PATH "Path to Irrlicht installation")

# Find Irrlicht (MUST happen before Chrono to define Irrlicht::Irrlicht target)
if(HYDROCHRONO_ENABLE_IRRLICHT)
	find_package(Irrlicht REQUIRED MODULE)
	message(STATUS "Found Irrlicht include dir: ${IRRLICHT_INCLUDE_DIR}")
endif()

# Now we can find Chrono (which assumes Irrlicht::Irrlicht already exists)
find_package(Chrono CONFIG REQUIRED)

# Verify required Chrono targets are available
if(NOT TARGET Chrono::Chrono_core)
	message(FATAL_ERROR "Chrono::Chrono_core target not found. Ensure Chrono was built with modern CMake target exports.")
endif()

# Enable Irrlicht support if requested and available
if(HYDROCHRONO_ENABLE_IRRLICHT)
	if(TARGET Chrono::Chrono_irrlicht)
		add_compile_definitions("HYDROCHRONO_HAVE_IRRLICHT=1")
	else()
		message(FATAL_ERROR "HYDROCHRONO_ENABLE_IRRLICHT is ON but Chrono::Chrono_irrlicht target not found. Ensure Chrono was built with Irrlicht support.")
	endif()
endif()

# Ensure we have a compatible Chrono version (check after find_package)
if(DEFINED CHRONO_VERSION)
	if(CHRONO_VERSION VERSION_LESS "9.0.1")
		message(WARNING "HydroChrono now requires Chrono ≥ 9.0.1 for modular target support. Found version: ${CHRONO_VERSION}")
	endif()
else()
	# If CHRONO_VERSION is not defined, assume it's a compatible version
	# since the modern targets are available
	message(STATUS "Chrono version not explicitly set, but modern targets are available - assuming compatible version")
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
		set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_uc} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${cfg})
		set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_uc} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${cfg})
		set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_uc} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${cfg})
	endforeach()
elseif(CMAKE_BUILD_TYPE)
	# Single-config generator with explicit build type
	string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_uc)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${build_type_uc} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${build_type_uc} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE})
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${build_type_uc} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE})
endif()

# Category-specific output folders for organization
set(HYDROCHRONO_DEMO_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/demos CACHE PATH "Output directory for demo executables")
set(HYDROCHRONO_TEST_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests CACHE PATH "Output directory for test executables")

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 5. Core Library Target ───────────────────────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

# Helper function to configure common HydroChrono target properties
# Now uses modern Chrono imported targets (Chrono ≥ v9.1.0)
function(configure_hydro_target tgt)
	target_compile_features(${tgt} PUBLIC cxx_std_17)
	
	target_include_directories(${tgt}
		PUBLIC
			$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
			$<INSTALL_INTERFACE:include>
	)
	
	# Enable position-independent code for static library linking
	set_target_properties(${tgt} PROPERTIES POSITION_INDEPENDENT_CODE ON)
	
	# Link to modern Chrono imported targets - these automatically propagate
	# include paths, compile flags, and link dependencies
	target_link_libraries(${tgt}
		PUBLIC
			Chrono::Chrono_core
			# Conditionally link Irrlicht component if enabled
			$<$<BOOL:${HYDROCHRONO_ENABLE_IRRLICHT}>:Chrono::Chrono_irrlicht>
	)
endfunction()

# Helper function to configure test environment (Windows DLL paths, etc.)
function(configure_test_environment)
	# Set up DLL search paths for Windows testing
	set(CHRONO_DLL_DIR "${Chrono_DIR}/../bin/Release")
	set(IRRLICHT_DLL_DIR "C:/libs/irrlicht-1.8.4/bin/Win64-VisualStudio")
	set(TEST_ENVIRONMENT "PATH=${CHRONO_DLL_DIR};${IRRLICHT_DLL_DIR};$ENV{PATH}" PARENT_SCOPE)
endfunction()

# ── Main HydroChrono Library ──
set(HYDROCHRONO_SOURCES
	src/h5fileinfo.cpp
	src/chloadaddedmass.cpp
	src/hydro_forces.cpp
	src/helper.cpp
	src/wave_types.cpp
    # src/simulation_logging.cpp  # removed in streamlined logging
	src/hydro_yaml_parser.cpp
	src/setup_hydro_from_yaml.cpp
    # logging consolidated: coordinator + CLI (logging.cpp), backend (logger_backend.cpp)
    src/utils/logger_backend.cpp
    src/utils/logging.cpp
    src/h5_writer.cpp
    src/simulation_exporter.cpp
)

# Create the library target and configure it
add_library(HydroChrono ${HYDROCHRONO_SOURCES})
configure_hydro_target(HydroChrono)

# Add HDF5 as private dependency (don't expose to users)
target_include_directories(HydroChrono PRIVATE ${HDF5_INCLUDE_DIRS})
target_link_libraries(HydroChrono PRIVATE ${HDF5_LIBRARIES})

# Library-specific compile definitions
target_compile_definitions(HydroChrono
	PUBLIC
		"CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\""
		"HYDROCHRONO_VERSION=\"${PROJECT_VERSION}\""
		"HYDROCHRONO_BUILD_TYPE=\"$<IF:$<CONFIG:Debug>,Debug,$<IF:$<CONFIG:Release>,Release,$<IF:$<CONFIG:RelWithDebInfo>,RelWithDebInfo,$<IF:$<CONFIG:MinSizeRel>,MinSizeRel,Unknown>>>>\""
	PRIVATE 
		$<$<BOOL:${HYDROCHRONO_ENABLE_LOGGING}>:"HYDROCHRONO_ENABLE_LOGGING=1">
)

# ═══════════════════════════════════════════════════════════════════════════════
# ─── 6. Auxiliary Targets (GUI, Tests, Demos) ─────────────────────────────────
# ═══════════════════════════════════════════════════════════════════════════════

# ── Main YAML-Driven CLI App ──
message(STATUS "HYDROCHRONO_ENABLE_YAML_RUNNER: ${HYDROCHRONO_ENABLE_YAML_RUNNER}")
if(HYDROCHRONO_ENABLE_YAML_RUNNER)
    add_executable(run_hydrochrono
        app/run_hydrochrono.cpp
        src/hydrochrono_runner/run_hydrochrono_from_yaml.cpp
        src/utils/misc_options.cpp
        src/utils/setup_parser.cpp
    )

    target_link_libraries(run_hydrochrono
        PRIVATE HydroChrono HydroChronoGUI Chrono::Chrono_parsers
    )

    target_include_directories(run_hydrochrono
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src/utils
    )

    set_target_properties(run_hydrochrono PROPERTIES
        OUTPUT_NAME "run_hydrochrono"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
else()
    message(STATUS "YAML runner disabled - not creating run_hydrochrono target")
endif()

# ── GUI Helper Library ──
# Separate library for Irrlicht visualization helpers
add_library(HydroChronoGUI src/gui/guihelper.cpp)
configure_hydro_target(HydroChronoGUI)

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
	
	# Configure test environment with DLL paths
	configure_test_environment()

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
