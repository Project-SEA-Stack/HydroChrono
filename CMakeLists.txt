# ===============================================================================
# ------ Prerequisites & Policies -----------------------------------------------
# ===============================================================================

# cmake_minimum_required(3.18.2): need CMP0091 policy + target properties support
cmake_minimum_required(VERSION 3.18.2)

# CMP0091: Enable MSVC runtime library selection via CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0091 NEW)
# CMP0074: Use <PackageName>_ROOT variables for find_package()
cmake_policy(SET CMP0074 NEW)

# Guard against in-source builds to prevent polluting source directory
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt.")
endif()

# ===============================================================================
# ------- Project Definition & Defaults -----------------------------------------
# ===============================================================================

project(HydroChrono 
	VERSION 0.3.1
	DESCRIPTION "Hydrodynamics for Project Chrono."
	LANGUAGES CXX
)

# ===============================================================================
# -------- Parse Project Metadata ----------------------------------------------
# ===============================================================================

# Read and parse project.meta file if present; otherwise provide sensible defaults
if(EXISTS "${CMAKE_SOURCE_DIR}/project.meta")
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
else()
    # Defaults when meta file is not present
    set(HYDROCHRONO_NAME "${PROJECT_NAME}")
    set(HYDROCHRONO_VERSION "${PROJECT_VERSION}")
    set(HYDROCHRONO_DESCRIPTION "${PROJECT_DESCRIPTION}")
    set(HYDROCHRONO_AUTHOR "")
    set(HYDROCHRONO_MAINTAINER "")
    set(HYDROCHRONO_URL "")
    set(HYDROCHRONO_LICENSE "")
    set(HYDROCHRONO_STATUS "")
endif()

# Display parsed metadata
message(STATUS "Project: ${HYDROCHRONO_NAME} v${HYDROCHRONO_VERSION}")
message(STATUS "Description: ${HYDROCHRONO_DESCRIPTION}")
message(STATUS "Author: ${HYDROCHRONO_AUTHOR}")
message(STATUS "Maintainer: ${HYDROCHRONO_MAINTAINER}")
message(STATUS "License: ${HYDROCHRONO_LICENSE}")
message(STATUS "Status: ${HYDROCHRONO_STATUS}")

# Generate version.h header from template
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in
               ${CMAKE_BINARY_DIR}/hydroc/version.h @ONLY)

# Add the cmake folder so the FindSphinx module is found
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Deterministic dependency discovery (avoid ambient PATH/registry drift)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF)

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

# ===============================================================================
# ------ User Options -----------------------------------------------------------
# ===============================================================================

option(HYDROCHRONO_ENABLE_TESTS "Enable tests" ON)
option(HYDROCHRONO_ENABLE_IRRLICHT "Enable irrlicht visualization library" OFF)
option(HYDROCHRONO_ENABLE_DEMOS "Enable demo executables" OFF)
option(HYDROCHRONO_ENABLE_YAML_RUNNER "Enable YAML-based CLI runner" OFF)
option(HYDROCHRONO_ENABLE_USER_DOC "User's documentation" OFF)
option(HYDROCHRONO_ENABLE_PROG_DOC "Programmer's documentation" OFF)
option(HYDROCHRONO_ENABLE_LOGGING "Enable debug logging" OFF)

# ===============================================================================
# --- 4. Build setup ------------------------------------------------------------
# ===============================================================================

message(STATUS "")

# Fix for VS 2017 15.8 and newer to handle alignment specification with Eigen
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

# -- HydroChrono data directory

message(STATUS "HydroChrono data directory")

# Destination directory for HydroChrono data in BUILD tree
set(HC_BUILD_DATA "${CMAKE_BINARY_DIR}/data")

# Destination directory for HydroChrono data in INSTALL tree
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(HC_INSTALL_DATA "data")
    set(HC_INSTALL_DEMO "bin")
    set(HC_INSTALL_PYTHON "bin")
else()
    set(HC_INSTALL_DATA "share/chrono/data")
    set(HC_INSTALL_DEMO "share/chrono/bin")
    set(HC_INSTALL_PYTHON "share/chrono/python")
endif()

# Copy data directory to BUILD tree
file(COPY ${CMAKE_SOURCE_DIR}/data/ DESTINATION ${HC_BUILD_DATA})
message(STATUS "  copied to: ${HC_BUILD_DATA}/")

# Install data directory
install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/ DESTINATION ${HC_INSTALL_DATA})
message(STATUS "  installed to: ${CMAKE_INSTALL_PREFIX}/${HC_INSTALL_DATA}/")

# -- Output Directory Structure --

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
message(STATUS "HydroChrono output directories")
set(HYDROCHRONO_DEMO_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/demos CACHE PATH "Output directory for demo executables")
set(HYDROCHRONO_TEST_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests CACHE PATH "Output directory for test executables")

message(STATUS "  demo output: ${HYDROCHRONO_DEMO_OUTPUT_DIR}")
message(STATUS "  test output: ${HYDROCHRONO_TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${HYDROCHRONO_DEMO_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${HYDROCHRONO_TEST_OUTPUT_DIR}")

# ===============================================================================
# ------- Find Dependencies -----------------------------------------------------
# ===============================================================================

# -- Chrono package --

message(STATUS "\n---- Find Chrono")

if(NOT Chrono_DIR)
    message("Specify the directory containing a CMake configuration file for Chrono")
    set(Chrono_DIR "" CACHE PATH "The directory containing a CMake configuration file for Chrono.")
    return()
endif()

find_package(Chrono
             CONFIG REQUIRED
             COMPONENTS Parsers
             OPTIONAL_COMPONENTS Irrlicht)

message(STATUS "----\n")

message(STATUS "Chrono targets: ${CHRONO_TARGETS}")

# Verify required Chrono targets are available
if(NOT TARGET Chrono::Chrono_core)
	message(FATAL_ERROR "Chrono::Chrono_core target not found. Ensure Chrono was built with modern CMake target exports.")
endif()
if(NOT TARGET Chrono::Chrono_parsers)
	message(FATAL_ERROR "Chrono::Chrono_parsers target not found. Ensure Chrono was built with modern CMake target exports.")
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

# To ensure ABI compatibility, use the same C++ standard as the one used to build Chrono
if (CHRONO_CXX_STANDARD)
  set(HC_CXX_STANDARD ${CHRONO_CXX_STANDARD})
  message(STATUS "Set C++ standard same as Chrono: ${HC_CXX_STANDARD}")
else()
  set(HC_CXX_STANDARD cxx_std_17)
  message(STATUS "Set C++ standard to: ${HC_CXX_STANDARD}")
endif()

# Enable Irrlicht support if requested and available
if(HYDROCHRONO_ENABLE_IRRLICHT AND NOT TARGET Chrono::Chrono_irrlicht)
  message(FATAL_ERROR "HYDROCHRONO_ENABLE_IRRLICHT is ON but Chrono::Chrono_irrlicht target not found. Ensure Chrono was built with Irrlicht support.")
endif()

# If on a Windows platform, copy the Chrono DLLs to the binary directory
add_CHRONO_DLLS_copy_command()

# -- OpenMP support --
find_package(OpenMP REQUIRED COMPONENTS CXX)

# -- HDF5 Integration --
find_package(HDF5 REQUIRED COMPONENTS CXX)

# ===============================================================================
# ----- Create HydroChrono configuration header ---------------------------------
# ===============================================================================

message(STATUS "Generate HydroChrono configuration headers")

set(CH_DATA_DIR "#define CHRONO_DATA_DIR \"${CHRONO_DATA_DIR}\"")

if(HYDROCHRONO_ENABLE_IRRLICHT)
  set(HC_HAS_IRRLICHT "#define HYDROCHRONO_HAVE_IRRLICHT")
else()
  set(HC_HAS_IRRLICHT "#undef HYDROCHRONO_HAVE_IRRLICHT")
endif()

# For BUILD tree
set(HC_DATA_DIR "#define HC_DATA_DIR \"${HC_BUILD_DATA}\"")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
               ${PROJECT_BINARY_DIR}/hydroc/config.h
               @ONLY)

# For INSTALL tree
set(HC_DATA_DIR "#define HC_DATA_DIR \"${CMAKE_INSTALL_PREFIX}/${HC_INSTALL_DATA}\"")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
               ${PROJECT_BINARY_DIR}/hydroc/config.h-install
               @ONLY)

install(FILES ${PROJECT_BINARY_DIR}/hydroc/config-install.h
        RENAME "config.h"
        DESTINATION include/chrono)

# ===============================================================================
# ------- Core Library Target ---------------------------------------------------
# ===============================================================================

# Helper function to configure common HydroChrono target properties
# Now uses modern Chrono imported targets (Chrono ≥ v9.1.0)
function(configure_hydro_target tgt)
	target_compile_features(${tgt} PUBLIC ${HC_CXX_STANDARD})
	
	list(APPEND HC_INCLUDES_BUILD ${PROJECT_SOURCE_DIR}/include)
	list(APPEND HC_INCLUDES_BUILD ${PROJECT_BINARY_DIR})
	target_include_directories(${tgt}
		PUBLIC
			"$<BUILD_INTERFACE:${HC_INCLUDES_BUILD}>"
			"$<INSTALL_INTERFACE:include>"
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
	# Always include our build output bin directories for all common configs
	set(HC_BIN_DIRS "${CMAKE_BINARY_DIR}/bin/Release;${CMAKE_BINARY_DIR}/bin/RelWithDebInfo;${CMAKE_BINARY_DIR}/bin/MinSizeRel;${CMAKE_BINARY_DIR}/bin/Debug")
	# Include HDF5 bin if available to stabilize test runtime on clean systems
	set(HDF5_DLL_DIR "")
	if(DEFINED HDF5_ROOT AND EXISTS "${HDF5_ROOT}/bin")
		set(HDF5_DLL_DIR "${HDF5_ROOT}/bin")
	endif()
	# Derive Irrlicht DLL dir from Irrlicht_ROOT provided by user config
	set(IRRLICHT_DLL_DIR "")
	if(DEFINED Irrlicht_ROOT AND EXISTS "${Irrlicht_ROOT}/bin/Win64-VisualStudio")
		set(IRRLICHT_DLL_DIR "${Irrlicht_ROOT}/bin/Win64-VisualStudio")
	elseif(DEFINED Irrlicht_ROOT AND EXISTS "${Irrlicht_ROOT}/bin")
		set(IRRLICHT_DLL_DIR "${Irrlicht_ROOT}/bin")
	endif()
	if(IRRLICHT_DLL_DIR)
		if(HDF5_DLL_DIR)
			set(TEST_ENVIRONMENT "PATH=${HC_BIN_DIRS};${CHRONO_DLL_DIR};${IRRLICHT_DLL_DIR};${HDF5_DLL_DIR};$ENV{PATH}" PARENT_SCOPE)
		else()
			set(TEST_ENVIRONMENT "PATH=${HC_BIN_DIRS};${CHRONO_DLL_DIR};${IRRLICHT_DLL_DIR};$ENV{PATH}" PARENT_SCOPE)
		endif()
	else()
		if(HDF5_DLL_DIR)
			set(TEST_ENVIRONMENT "PATH=${HC_BIN_DIRS};${CHRONO_DLL_DIR};${HDF5_DLL_DIR};$ENV{PATH}" PARENT_SCOPE)
		else()
			set(TEST_ENVIRONMENT "PATH=${HC_BIN_DIRS};${CHRONO_DLL_DIR};$ENV{PATH}" PARENT_SCOPE)
		endif()
	endif()
endfunction()

# -- Main HydroChrono Library --
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

set(HYDROCHRONO_HEADERS
    ${PROJECT_BINARY_DIR}/hydroc/config.h
    ${PROJECT_BINARY_DIR}/hydroc/version.h
    include/hydroc/h5_writer.h
    include/hydroc/h5fileinfo.h
    include/hydroc/helper.h
    include/hydroc/hydro_forces.h
    include/hydroc/logging.h
)

# Create the library target and configure it
add_library(HydroChrono ${HYDROCHRONO_SOURCES} ${HYDROCHRONO_HEADERS})
configure_hydro_target(HydroChrono)

# Add OpenMP
target_link_libraries(HydroChrono PUBLIC OpenMP::OpenMP_CXX)

# Add HDF5 (assume first latest HDF5 version 1.14.6)
if(TARGET hdf5-static)
  message(STATUS "Linking static libraries hdf5-static")
  target_link_libraries(HydroChrono PUBLIC hdf5-static)
  target_link_libraries(HydroChrono PUBLIC hdf5_cpp-static)
  target_link_libraries(HydroChrono PUBLIC hdf5_hl-static)
  target_link_libraries(HydroChrono PUBLIC hdf5_hl_cpp-static)
  target_link_libraries(HydroChrono PUBLIC hdf5_tools-static)
elseif(TARGET hdf5-shared)
  message(STATUS "Linking shared libraries hdf5-shared")
  target_link_libraries(HydroChrono PUBLIC hdf5-shared)
  target_link_libraries(HydroChrono PUBLIC hdf5_cpp-shared)
  target_link_libraries(HydroChrono PUBLIC hdf5_hl-shared)
  target_link_libraries(HydroChrono PUBLIC hdf5_hl_cpp-shared)
  target_link_libraries(HydroChrono PUBLIC hdf5_tools-shared)
elseif(TARGET HDF5::HDF5)
  message(STATUS "Linking HDF5::HDF5")
  target_link_libraries(HydroChrono PUBLIC HDF5::HDF5)
else()
  message(STATUS "No HDF5 target found")
  target_compile_definitions(HydroChrono PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${HDF5_COMPILE_DEFS}>)
  target_include_directories(HydroChrono PUBLIC ${HDF5_INCLUDE_DIRS})
  target_link_libraries(HydroChrono PUBLIC ${HDF5_CXX_LIBRARIES})
endif()

target_include_directories(HydroChrono
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Library-specific compile definitions
target_compile_definitions(HydroChrono
	PUBLIC
		"HYDROCHRONO_VERSION=\"${PROJECT_VERSION}\""
		"HYDROCHRONO_BUILD_TYPE=\"$<IF:$<CONFIG:Debug>,Debug,$<IF:$<CONFIG:Release>,Release,$<IF:$<CONFIG:RelWithDebInfo>,RelWithDebInfo,$<IF:$<CONFIG:MinSizeRel>,MinSizeRel,Unknown>>>>\""
	PRIVATE 
		$<$<BOOL:${HYDROCHRONO_ENABLE_LOGGING}>:"HYDROCHRONO_ENABLE_LOGGING=1">
)

# ===============================================================================
# ------- Auxiliary Targets (GUI, Tests, Demos) ---------------------------------
# ===============================================================================

# -- Main YAML-Driven CLI App --
message(STATUS "HYDROCHRONO_ENABLE_YAML_RUNNER: ${HYDROCHRONO_ENABLE_YAML_RUNNER}")
if(HYDROCHRONO_ENABLE_YAML_RUNNER)
    add_executable(run_hydrochrono
        app/run_hydrochrono.cpp
        src/hydrochrono_runner/run_hydrochrono_from_yaml.cpp
        src/utils/misc_options.cpp
        src/utils/setup_parser.cpp
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

    set_target_properties(run_hydrochrono PROPERTIES
        OUTPUT_NAME "run_hydrochrono"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
else()
    message(STATUS "YAML runner disabled - not creating run_hydrochrono target")
endif()

# ====================
# Irrlicht GUI helper
# ====================

# Separate library for Irrlicht visualization helpers
add_library(HydroChronoGUI src/gui/guihelper.cpp)
configure_hydro_target(HydroChronoGUI)

# ====================
# DEMOS
# ====================

if(HYDROCHRONO_ENABLE_DEMOS)
    add_subdirectory(demos)
endif()

# ===================
# TESTS
# ===================

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

# ===============================================================================
# ------ Installation -----------------------------------------------------------
# ===============================================================================

include(GNUInstallDirs)
option(HC_INSTALL_DEV_KIT "Install CMake dev package (headers/libs)" OFF)

if(HC_INSTALL_DEV_KIT)
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
endif()

if(HC_INSTALL_DEV_KIT)
    # Export targets for build tree usage (without installation)
    export(EXPORT HydroChronoTargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/HydroChronoTargets.cmake"
        NAMESPACE HydroChrono::
    )
endif()

if(MSVC)
    # Windows: Enable debug symbols in Release builds with MSVC
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi /O2")
else()
    # Non-MSVC (Linux, macOS): Use -g for debug symbols
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O2")
endif()


# ------------------------------------------------------------------------------
# Runtime installer and ZIP packaging for releases
# ------------------------------------------------------------------------------

# Flat install tree for public distribution
option(HC_INSTALL_DEV_DEMOS "Install developer demo executables" OFF)

# Install main CLI only (if built)
if(TARGET run_hydrochrono)
	install(TARGETS run_hydrochrono
		RUNTIME DESTINATION bin COMPONENT runtime)
endif()

# Collect runtime DLLs from imported targets when available
set(HC_DLLS)
foreach(tgt IN ITEMS Chrono::Chrono_core Chrono::Chrono_irrlicht Chrono::Chrono_parsers Chrono::ChronoModels_robot yaml-cpp yaml-cpp::yaml-cpp)
	if(TARGET ${tgt})
		list(APPEND HC_DLLS "$<TARGET_FILE:${tgt}>")
	endif()
endforeach()

# Irrlicht.dll overridable from build system; default derived from Irrlicht_ROOT
set(IRRLICHT_DLL_PATH_DEFAULT "${Irrlicht_ROOT}/bin/Win64-VisualStudio/Irrlicht.dll")
set(IRRLICHT_DLL_PATH "${IRRLICHT_DLL_PATH_DEFAULT}" CACHE FILEPATH "Path to Irrlicht.dll")
if(EXISTS "${IRRLICHT_DLL_PATH}")
	list(APPEND HC_DLLS "${IRRLICHT_DLL_PATH}")
endif()

# YAML-CPP runtime DLL overridable from build system
set(YAML_CPP_DLL_PATH "" CACHE FILEPATH "Path to yaml-cpp runtime DLL (yaml-cpp.dll)")
if(EXISTS "${YAML_CPP_DLL_PATH}")
	list(APPEND HC_DLLS "${YAML_CPP_DLL_PATH}")
endif()

if(HC_DLLS)
	install(FILES ${HC_DLLS} DESTINATION bin COMPONENT runtime)
endif()

# Ensure yaml-cpp.dll is part of the runtime component in the ZIP (OPTIONAL handles missing paths)
install(FILES
    "${Chrono_DIR}/../bin/Release/yaml-cpp.dll"
    "${Chrono_DIR}/../bin/RelWithDebInfo/yaml-cpp.dll"
    "${Chrono_DIR}/../bin/MinSizeRel/yaml-cpp.dll"
    "${Chrono_DIR}/../bin/Debug/yaml-cpp.dll"
    "${CMAKE_BINARY_DIR}/bin/Release/yaml-cpp.dll"
    "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo/yaml-cpp.dll"
    "${CMAKE_BINARY_DIR}/bin/MinSizeRel/yaml-cpp.dll"
    "${CMAKE_BINARY_DIR}/bin/Debug/yaml-cpp.dll"
    DESTINATION bin COMPONENT runtime OPTIONAL)

# HDF5 runtime DLLs (prefer imported targets, fallback to bin under HDF5_ROOT)
set(HDF5_DLLS)
foreach(tgt IN ITEMS HDF5::HDF5 HDF5::HDF5_CXX hdf5::hdf5 hdf5::hdf5_cpp hdf5::hdf5-shared hdf5::hdf5_cpp-shared)
	if(TARGET ${tgt})
		list(APPEND HDF5_DLLS "$<TARGET_FILE:${tgt}>")
	endif()
endforeach()
if(HDF5_DLLS)
	install(FILES ${HDF5_DLLS} DESTINATION bin COMPONENT runtime)
else()
	if(DEFINED HDF5_ROOT)
		install(CODE "
		  file(GLOB _H5_DLLS \"${HDF5_ROOT}/bin/*.dll\")
		  foreach(_f IN LISTS _H5_DLLS)
		    if(EXISTS \"${_f}\")
		      file(INSTALL DESTINATION \"${CMAKE_INSTALL_PREFIX}/bin\" TYPE FILE FILES \"${_f}\")
		    endif()
		  endforeach()
		")
	endif()
endif()

# Fallback: copy all Chrono-built DLLs for the active config into bin
install(CODE "
  file(GLOB _ALL_DLLS \"${Chrono_DIR}/../bin/${CMAKE_INSTALL_CONFIG_NAME}/*.dll\")
  foreach(_f IN LISTS _ALL_DLLS)
    if(EXISTS \"${_f}\")
      file(INSTALL DESTINATION \"${CMAKE_INSTALL_PREFIX}/bin\" TYPE FILE FILES \"${_f}\")
    endif()
  endforeach()
")

# Also copy any DLLs produced or staged in this project's build bin (e.g., yaml-cpp.dll)
install(CODE "
  file(GLOB _HC_DLLS \"${CMAKE_BINARY_DIR}/bin/${CMAKE_INSTALL_CONFIG_NAME}/*.dll\")
  foreach(_f IN LISTS _HC_DLLS)
    if(EXISTS \"${_f}\")
      file(INSTALL DESTINATION \"${CMAKE_INSTALL_PREFIX}/bin\" TYPE FILE FILES \"${_f}\")
    endif()
  endforeach()
")

# Removed fragile fallback DLL copy to avoid install script parse errors. The
# release DLLs should be resolved via imported targets or IRRLICHT_DLL_PATH.

# Optional runtime data folder
if(EXISTS "${PROJECT_SOURCE_DIR}/data")
	install(DIRECTORY ${PROJECT_SOURCE_DIR}/data/ DESTINATION data COMPONENT runtime)
endif()

# Chrono visual assets (skybox, colormaps) for GUI
if(DEFINED CHRONO_DATA_DIR AND EXISTS "${CHRONO_DATA_DIR}")
	if(EXISTS "${CHRONO_DATA_DIR}/skybox")
		install(DIRECTORY "${CHRONO_DATA_DIR}/skybox" DESTINATION data COMPONENT runtime)
	endif()
	if(EXISTS "${CHRONO_DATA_DIR}/colormaps")
		install(DIRECTORY "${CHRONO_DATA_DIR}/colormaps" DESTINATION data COMPONENT runtime)
	endif()
endif()

# Public, user-facing regression test suite only
install(DIRECTORY ${PROJECT_SOURCE_DIR}/tests/regression/run_hydrochrono
        DESTINATION tests COMPONENT python-tests
		PATTERN "__pycache__" EXCLUDE
		PATTERN "*.pyc" EXCLUDE)

# Simple runner script to execute tests from the installed tree
install(PROGRAMS ${PROJECT_SOURCE_DIR}/scripts/RUN-TESTS.ps1
		DESTINATION tests COMPONENT python-tests)

# Optional developer demo executables (OFF by default)
if(HC_INSTALL_DEV_DEMOS)
	install(DIRECTORY ${HYDROCHRONO_DEMO_OUTPUT_DIR}/$<CONFIG>/
			DESTINATION bin/tests/dev
			FILES_MATCHING PATTERN "demo_*.exe")
endif()

# MSVC runtime DLLs and ZIP packaging via CPack
 include(InstallRequiredSystemLibraries)
 set(CPACK_GENERATOR "ZIP")
 set(CPACK_PACKAGE_NAME "HydroChrono")
 set(CPACK_COMPONENTS_ALL runtime python-tests dev-demos)
 include(CPack)

# Optionally include Python runtime DLL if Chrono::Parsers depends on it
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND OR Python3_FOUND)
    get_filename_component(_PY_DIR "${Python3_EXECUTABLE}" DIRECTORY)
    if(DEFINED Python3_VERSION_MAJOR AND DEFINED Python3_VERSION_MINOR)
        set(_PY_DLL_NAME "python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
        set(_PY_DLL_PATH "${_PY_DIR}/${_PY_DLL_NAME}")
        if(EXISTS "${_PY_DLL_PATH}")
            install(FILES "${_PY_DLL_PATH}" DESTINATION bin COMPONENT runtime)
        endif()
    endif()
endif()
