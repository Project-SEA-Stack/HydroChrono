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
    VERSION 0.3.2
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
option(HYDROCHRONO_ENABLE_VSG "Enable VSG visualization library" OFF)
option(HYDROCHRONO_ENABLE_DEMOS "Enable demo executables" OFF)
option(HYDROCHRONO_ENABLE_YAML_RUNNER "Enable YAML-based CLI runner" OFF)
option(HYDROCHRONO_ENABLE_USER_DOC "User's documentation" OFF)
option(HYDROCHRONO_ENABLE_PROG_DOC "Programmer's documentation" OFF)
option(HYDROCHRONO_ENABLE_LOGGING "Enable debug logging" OFF)

# ===============================================================================
# ------ Build setup ------------------------------------------------------------
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
install(DIRECTORY ${HC_BUILD_DATA} DESTINATION ${HC_INSTALL_DATA})
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
             OPTIONAL_COMPONENTS Irrlicht VSG)

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

# Enable VSG support if requested and available
if(HYDROCHRONO_ENABLE_VSG AND NOT TARGET Chrono::Chrono_vsg)
  message(FATAL_ERROR "HYDROCHRONO_ENABLE_VSG is ON but Chrono::Chrono_vsg target not found. Ensure Chrono was built with VSG support.")
endif()

# If on a Windows platform, copy the Chrono DLLs to the binary directory
add_CHRONO_DLLS_copy_command()

# Copy selected Chrono data directories to the HydroChrono data directory
if(EXISTS "${CHRONO_DATA_DIR}")
    if(EXISTS "${CHRONO_DATA_DIR}/colormaps")
        file(COPY ${CHRONO_DATA_DIR}/colormaps DESTINATION ${HC_BUILD_DATA}/chrono)
    endif()

    if(EXISTS "${CHRONO_DATA_DIR}/skybox")
        file(COPY ${CHRONO_DATA_DIR}/skybox DESTINATION ${HC_BUILD_DATA}/chrono)
    endif()

    if(EXISTS "${CHRONO_DATA_DIR}/vsg")
        file(COPY ${CHRONO_DATA_DIR}/vsg DESTINATION ${HC_BUILD_DATA}/chrono)
    endif()

    if(EXISTS "${CHRONO_DATA_DIR}/logo_chrono_alpha.png")
        file(COPY ${CHRONO_DATA_DIR}/logo_chrono_alpha.png DESTINATION ${HC_BUILD_DATA}/chrono)
    endif()
endif()

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

if(HYDROCHRONO_ENABLE_VSG)
  set(HC_HAS_VSG "#define HYDROCHRONO_HAVE_VSG")
else()
  set(HC_HAS_VSG "#undef HYDROCHRONO_HAVE_VSG")
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

# ===============================================================================
# ------- Core Library Target ---------------------------------------------------
# ===============================================================================

list(APPEND HC_INCLUDES_BUILD ${PROJECT_SOURCE_DIR}/include)
list(APPEND HC_INCLUDES_BUILD ${PROJECT_BINARY_DIR})

# -- Main HydroChrono Library --
set(HC_SOURCES
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

set(HC_HEADERS
    ${PROJECT_BINARY_DIR}/hydroc/config.h
    ${PROJECT_BINARY_DIR}/hydroc/version.h
    include/hydroc/h5_writer.h
    include/hydroc/h5fileinfo.h
    include/hydroc/helper.h
    include/hydroc/hydro_forces.h
    include/hydroc/logging.h
)

# Create the library target and configure it
add_library(HydroChrono ${HC_SOURCES} ${HC_HEADERS})

target_compile_features(HydroChrono PUBLIC ${HC_CXX_STANDARD})
target_include_directories(HydroChrono
    PUBLIC
        "$<BUILD_INTERFACE:${HC_INCLUDES_BUILD}>"
        "$<INSTALL_INTERFACE:include>"
)
set_target_properties(HydroChrono PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(HydroChrono PUBLIC Chrono::Chrono_core)

# Add OpenMP
target_link_libraries(HydroChrono PUBLIC OpenMP::OpenMP_CXX)

# Add HDF5 (assume first latest HDF5 version 1.14.6 and prefer static library)
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
  list(APPEND HDF5_TARGETS hdf5-shared)
  list(APPEND HDF5_TARGETS hdf5_cpp-shared)
  list(APPEND HDF5_TARGETS hdf5_hl-shared)
  list(APPEND HDF5_TARGETS hdf5_hl_cpp-shared)
  list(APPEND HDF5_TARGETS hdf5_tools-shared)
elseif(TARGET HDF5::HDF5)
  message(STATUS "Linking HDF5::HDF5")
  target_link_libraries(HydroChrono PUBLIC HDF5::HDF5)
  list(APPEND HDF5_TARGETS HDF5::HDF5)
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
# ------- Visualization library -------------------------------------------------
# ===============================================================================

set(HCGUI_HEADERS
    include/hydroc/gui/guihelper.h
    src/gui/guihelper_impl.h
)

set(HCGUI_SOURCES
    src/gui/guihelper.cpp
    src/gui/guihelperIRR.cpp
    src/gui/guihelperVSG.cpp
)

add_library(HydroChronoGUI ${HCGUI_SOURCES} ${HCGUI_HEADERS})

target_compile_features(HydroChronoGUI PUBLIC ${HC_CXX_STANDARD})
target_include_directories(HydroChronoGUI
    PUBLIC
        "$<BUILD_INTERFACE:${HC_INCLUDES_BUILD}>"
        "$<INSTALL_INTERFACE:include>"
)
set_target_properties(HydroChronoGUI PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(HydroChronoGUI
    PUBLIC
        Chrono::Chrono_core
        # Conditionally link Irrlicht and VSG components if enabled
        $<$<BOOL:${HYDROCHRONO_ENABLE_IRRLICHT}>:Chrono::Chrono_irrlicht>
        $<$<BOOL:${HYDROCHRONO_ENABLE_VSG}>:Chrono::Chrono_vsg>
)

# ===============================================================================
# ------- Auxiliary Targets (app, tests, demos) ---------------------------------
# ===============================================================================

# Helper function to configure test environment (Windows DLL paths, etc.)
function(configure_test_environment)
    message(STATUS "Configure test environment")
    
    # HydroChrono DLL paths
    set(HC_DLL_DIRS "${CMAKE_BINARY_DIR}/bin/Release;${CMAKE_BINARY_DIR}/bin/RelWithDebInfo;${CMAKE_BINARY_DIR}/bin/MinSizeRel;${CMAKE_BINARY_DIR}/bin/Debug")
    message(STATUS "  HydroChrono DLL directories:      ${HC_DLL_DIRS}")
    list(APPEND DLL_DIRS ${HC_DLL_DIRS})

    # Chrono DLL paths
    get_target_property(CHRONO_CORE_REL_DLL Chrono::Chrono_core IMPORTED_LOCATION_RELEASE)
    if(CHRONO_CORE_REL_DLL)
        get_filename_component(dir ${CHRONO_CORE_REL_DLL} DIRECTORY)
        message(STATUS "  Chrono release DLL directory:     ${dir}")
        list(APPEND DLL_DIRS ${dir})
    endif()
    get_target_property(CHRONO_CORE_DBG_DLL Chrono::Chrono_core IMPORTED_LOCATION_DEBUG)
    if(CHRONO_CORE_DBG_DLL)
        get_filename_component(dir ${CHRONO_CORE_DBG_DLL} DIRECTORY)
        message(STATUS "  Chrono debug DLL directory:       ${dir}")
        list(APPEND DLL_DIRS ${dir})
    endif()

    # HDF5 DLL path
    if(TARGET hdf5-static)
        message(STATUS "  No HDF5 DLL (hdf5-static)")
    elseif(TARGET hdf5-shared)
        get_target_property(dll hdf5-shared IMPORTED_LOCATION_RELEASE)
        get_filename_component(dir ${dll} DIRECTORY)
        message(STATUS "  HDF5 DLL directory (hdf5-shared): ${dir}")
        list(APPEND DLL_DIRS ${dir})
    elseif(TARGET HDF5::HDF5)
        get_target_property(dll HDF5::HFDF5 IMPORTED_LOCATION_RELEASE)
        get_filename_component(dir ${dll} DIRECTORY)
        message(STATUS "  HDF5 DLL directory (HDF5::HDF5):  ${dir}")
        list(APPEND DLL_DIRS ${dir})
    elseif(DEFINED HDF5_ROOT AND EXISTS "${HDF5_ROOT}/bin")
        message(STATUS "  HDF5 DLL directory (HDF5_ROOT):   ${HDF5_ROOT}/bin")
        list(APPEND DLL_DIRS ${HDF5_ROOT}/bin)
    else()
        message(STATUS "  Cannot infer HDF5 DLL directory")
    endif()

    # Irrlicht DLL path
    if(HYDROCHRONO_ENABLE_IRRLICHT)
        message(STATUS "  Irrlicht DLL directory:           ${Irrlicht_DLL_DIR}")
        list(APPEND DLL_DIRS ${Irrlicht_DLL_DIR})
    endif()

    # VSG DLL path
    if(HYDROCHRONO_ENABLE_VSG)
        message(STATUS "  VSG DLL directory:                ${VSG_DLL_DIR}")
        list(APPEND DLL_DIRS ${VSG_DLL_DIR})
    endif()

    set(TEST_ENVIRONMENT "PATH=${DLL_DIRS};$ENV{PATH}" PARENT_SCOPE)
endfunction()

# YAML-driven CLI app
if(HYDROCHRONO_ENABLE_YAML_RUNNER)
    add_subdirectory(app)
else()
    message(STATUS "YAML runner disabled - not creating run_hydrochrono target")
endif()

# Demos
if(HYDROCHRONO_ENABLE_DEMOS)
    add_subdirectory(demos)
endif()

# Tests
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
    if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        configure_test_environment()
    endif()

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
    
    # Install config.h header
    install(FILES ${PROJECT_BINARY_DIR}/hydroc/config.h-install
        RENAME "config.h"
        DESTINATION include/hydroc)

    # Install version.h header
    install(FILES ${PROJECT_BINARY_DIR}/hydroc/version.h
        DESTINATION include/hydroc)

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
    install(TARGETS run_hydrochrono  CONFIGURATIONS Release DESTINATION bin COMPONENT runtime)
endif()

# On Windows, install DLLs
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

    # Chrono DLLs
    foreach(tgt ${CHRONO_TARGETS})
        get_target_property(tgt_DLL ${tgt} IMPORTED_LOCATION_RELEASE)
        get_target_property(tgt_DLL_d ${tgt} IMPORTED_LOCATION_DEBUG)
        if(EXISTS ${tgt_DLL})
            install(FILES ${tgt_DLL} DESTINATION bin COMPONENT runtime)
        endif()
        if(EXISTS ${tgt_DLL_d})
            install(FILES ${tgt_DLL_d} DESTINATION bin COMPONENT runtime)
        endif()
    endforeach()

    # HDF5 DLLs
    foreach(tgt ${HDF5_TARGETS})
        get_target_property(tgt_DLL ${tgt} IMPORTED_LOCATION_RELEASE)
        if(EXISTS ${tgt_DLL})
            install(FILES ${tgt_DLL} DESTINATION bin COMPONENT runtime)
        endif()
    endforeach()
    
    # Python DLLs, if Chrono::Parsers depends on it
    if(CHRONO_PARSERS_PYTHON)
        find_package(Python3 QUIET COMPONENTS Interpreter Development)
        if(Python3_Interpreter_FOUND AND Python3_Development_FOUND)
            get_target_property(tgt_DLL Python3::Python IMPORTED_LOCATION_RELEASE)
            get_target_property(tgt_DLL_d Python3::Python IMPORTED_LOCATION_DEBUG)
            if(EXISTS ${tgt_DLL})
                install(FILES ${tgt_DLL} DESTINATION bin COMPONENT runtime)
            endif()
            if(EXISTS ${tgt_DLL_d})
                install(FILES ${tgt_DLL_d} DESTINATION bin COMPONENT runtime)
            endif()
        endif()
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

# MSVC runtime DLLs and ZIP packaging via CPack
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "HydroChrono")
set(CPACK_COMPONENTS_ALL runtime python-tests dev-demos)
include(CPack)
