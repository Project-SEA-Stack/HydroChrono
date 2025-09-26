#--------------------------------------------------------------
# CMake file for HydroChrono library and its demos
# Eventually will add SWIG and python lines to generate python
# library from C++ code
#--------------------------------------------------------------

cmake_minimum_required(VERSION 3.18.2)
cmake_policy(SET CMP0091 NEW)


# guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

# Default build type to use if none was specified
if(NOT DEFINED HYDROCHRONO_DEFAULT_BUILD_TYPE)
	set(HYDROCHRONO_DEFAULT_BUILD_TYPE "Release")
endif()

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${HYDROCHRONO_DEFAULT_BUILD_TYPE}' as none was specified.")

	set(CMAKE_BUILD_TYPE ${HYDROCHRONO_DEFAULT_BUILD_TYPE}
	    CACHE STRING "Choose the type of build." FORCE)
	mark_as_advanced(CMAKE_BUILD_TYPE)

	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

#--------------------------------------------------------------
# Modify the project name if you want: 
#--------------------------------------------------------------

project(HydroChrono 
	VERSION 0.2.6
	DESCRIPTION "A hydrodynamic library for use with Project Chrono."
	LANGUAGES CXX
)

# Add the cmake folder so the FindSphinx module is found
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

#--------------------------------------------------------------
# Important! To ensure ABI compatibility, use the same C++ standard
# as the one used to build the Chrono libraries.
#--------------------------------------------------------------

set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD ${CHRONO_CXX_STANDARD})

# ===================
# OPTIONS
# ===================
option (HYDROCHRONO_ENABLE_TESTS "Enable tests" ON)
option (HYDROCHRONO_ENABLE_IRRLICHT "Enable irrlicht visualization library" ON)
option (HYDROCHRONO_ENABLE_DEMOS "Enable demo executables" ON)
option (HYDROCHRONO_ENABLE_USER_DOC "User's documentation" OFF)
option (HYDROCHRONO_ENABLE_PROG_DOC "Programmer's documentation" OFF)
option (HYDROCHRONO_ENABLE_LOGGING "Enable debug logging" OFF)


# find required packages and libraries to make HydroChrono library
set (LIB_TYPE STATIC) # or SHARED
string(TOLOWER ${LIB_TYPE} SEARCH_TYPE)

if(HYDROCHRONO_ENABLE_IRRLICHT)
	# If irrlicht is required chrono library must be compiled with irrlicht
	find_package(Chrono COMPONENTS Irrlicht CONFIG REQUIRED)
	add_compile_definitions(HYDROCHRONO_HAVE_IRRLICHT=1)
else(HYDROCHRONO_ENABLE_IRRLICHT)
	find_package(Chrono CONFIG REQUIRED)
endif(HYDROCHRONO_ENABLE_IRRLICHT)

set(HDF5_USE_STATIC_LIBRARIES ON)

# Configure HDF5 to find its dependencies from its installation
set(HDF5_ROOT "${HDF5_ROOT}" CACHE PATH "HDF5 root directory")

find_package(HDF5 REQUIRED COMPONENTS CXX)

# ---- Eigen: support both CONFIG and module modes ----
find_package(Eigen3 QUIET CONFIG)
if (NOT Eigen3_FOUND)
  find_package(Eigen3 REQUIRED)  # module mode -> EIGEN3_INCLUDE_DIR
endif()

# ---- Chrono include path: prefer imported target; fall back to vars; fall back to build tree ----
set(HC_CHRONO_INCLUDE_DIRS "")

# Prefer INTERFACE includes from an imported target, if provided
if (TARGET Chrono::Chrono_core)
  get_target_property(HC_CHRONO_INCLUDE_DIRS Chrono::Chrono_core INTERFACE_INCLUDE_DIRECTORIES)
endif()

# Fallback to legacy variable if the package sets it
if (NOT HC_CHRONO_INCLUDE_DIRS AND DEFINED CHRONO_INCLUDE_DIRS)
  set(HC_CHRONO_INCLUDE_DIRS ${CHRONO_INCLUDE_DIRS})
endif()

# Derive from Chrono_DIR when using Chrono from a build tree (headers live under <chrono_root>/src)
if (NOT HC_CHRONO_INCLUDE_DIRS AND DEFINED Chrono_DIR)
  get_filename_component(_CHR_CMAKE_DIR "${Chrono_DIR}" ABSOLUTE)     # .../chrono/build/cmake
  get_filename_component(_CHR_BUILD_DIR "${_CHR_CMAKE_DIR}" DIRECTORY)# .../chrono/build
  get_filename_component(_CHR_ROOT      "${_CHR_BUILD_DIR}" DIRECTORY)# .../chrono
  set(_CHR_INC_CAND "${_CHR_ROOT}/src")
  if (EXISTS "${_CHR_INC_CAND}/chrono/core/ChMatrix.h")
    set(HC_CHRONO_INCLUDE_DIRS "${_CHR_INC_CAND}")
  endif()
endif()




#-----------------------------------------------------------------------------
# Fix for VS 2017 15.8 and newer to handle alignment specification with Eigen
#-----------------------------------------------------------------------------

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  if(MSVC AND ${MSVC_VERSION} GREATER_EQUAL 1915)
    add_compile_definitions( "ENABLE_EXTENDED_ALIGNED_STORAGE" )
  endif()
endif()

#--------------------------------------------------------------
# Tweaks to disable some warnings with MSVC
#--------------------------------------------------------------
if(MSVC)
	# avoids deprecation warnings
    add_definitions(
		-D_CRT_SECURE_NO_DEPRECATE  
		-D_SCL_SECURE_NO_DEPRECATE
		-DNOMINMAX
	)
    add_compile_options(/wd4275)                   # disable warnings triggered by Irrlicht
    add_compile_options(/wd4251)                   # class needs to have dll-interface
endif()

# --------------------------------------------------
# Global output directory structure
# --------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Replicate for multi-config generators (e.g. Visual Studio, Xcode, Ninja Multi-Config)
if(CMAKE_CONFIGURATION_TYPES)
  foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${cfg}" cfg_uc)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_uc} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_uc} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_uc} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
  endforeach()
endif()

# Category-specific output folders
set(HYDROCHRONO_DEMO_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/demos CACHE PATH "Output directory for demo executables")
set(HYDROCHRONO_TEST_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests CACHE PATH "Output directory for test executables")

# =======================
# HydroChrono Library
# =======================
set(HYDROCHRONO_SOURCES
  
	src/h5fileinfo.cpp
	src/chloadaddedmass.cpp
	src/hydro_forces.cpp
	src/helper.cpp
	src/wave_types.cpp
	src/logging.cpp

)

add_library(HydroChrono STATIC ${HYDROCHRONO_SOURCES} )

target_compile_features(HydroChrono PUBLIC cxx_std_17)

# target_include_directories(
	# HydroChrono

	# PUBLIC
	# $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	# $<INSTALL_INTERFACE:include>
	# ${CHRONO_INCLUDE_DIRS}

	# PRIVATE
	# ${CMAKE_CURRENT_SOURCE_DIR}/
	# ${CMAKE_CURRENT_SOURCE_DIR}/include
	# ${HDF5_INCLUDE_DIRS}
# )

target_include_directories(HydroChrono
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${HC_CHRONO_INCLUDE_DIRS}     # <— ensures chrono/ headers are visible
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${HDF5_INCLUDE_DIRS}
)

target_compile_definitions(HydroChrono
	PUBLIC
		CHRONO_DATA_DIR=\"${CHRONO_DATA_DIR}\"
		HYDROCHRONO_VERSION=\"${PROJECT_VERSION}\"
		HYDROCHRONO_BUILD_TYPE=\"$<IF:$<CONFIG:Debug>,Debug,$<IF:$<CONFIG:Release>,Release,$<IF:$<CONFIG:RelWithDebInfo>,RelWithDebInfo,$<IF:$<CONFIG:MinSizeRel>,MinSizeRel,Unknown>>>>\"
	PRIVATE 
		$<$<BOOL:${HYDROCHRONO_ENABLE_LOGGING}>:HYDROCHRONO_ENABLE_LOGGING=1>
)

target_compile_options(HydroChrono BEFORE 
	PUBLIC
		${CHRONO_CXX_FLAGS}
)

target_link_options(HydroChrono BEFORE 
	PUBLIC
		${CHRONO_LINKER_FLAGS}
)

set_target_properties(HydroChrono
	PROPERTIES
	POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(HydroChrono
  PUBLIC
    ${CHRONO_TARGETS}             # Chrono::Chrono_core;Chrono::Chrono_irrlicht;...
    $<$<TARGET_EXISTS:Eigen3::Eigen>:Eigen3::Eigen>
  PRIVATE
    ${HDF5_LIBRARIES}
)

# If Eigen is module-mode (no imported target), include its dir
if (NOT TARGET Eigen3::Eigen AND DEFINED EIGEN3_INCLUDE_DIR)
  target_include_directories(HydroChrono PUBLIC ${EIGEN3_INCLUDE_DIR})
endif()

# ====================
# Irrlicht GUI helper
# ====================

add_library(HydroChronoGUI)

target_sources(
	HydroChronoGUI

	PUBLIC
	src/gui/guihelper.cpp
)

target_compile_features(HydroChronoGUI PUBLIC cxx_std_17)


target_include_directories(HydroChronoGUI
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${HC_CHRONO_INCLUDE_DIRS}
)

target_compile_options(HydroChronoGUI BEFORE
	PUBLIC
		${CHRONO_CXX_FLAGS}
)

target_link_options(HydroChronoGUI BEFORE
	PUBLIC
		${CHRONO_LINKER_FLAGS}
)

target_link_libraries(HydroChronoGUI
  PUBLIC
    ${CHRONO_TARGETS}
    $<$<TARGET_EXISTS:Eigen3::Eigen>:Eigen3::Eigen>
)

if (NOT TARGET Eigen3::Eigen AND DEFINED EIGEN3_INCLUDE_DIR)
  target_include_directories(HydroChronoGUI PUBLIC ${EIGEN3_INCLUDE_DIR})
endif()



# ====================
# DEMOS
# ====================
if(HYDROCHRONO_ENABLE_DEMOS)
	add_subdirectory(demos)
endif(HYDROCHRONO_ENABLE_DEMOS)


# ====================
# TESTS
# ====================
if(HYDROCHRONO_ENABLE_TESTS)

	if(DEFINED ENV{HYDROCHRONO_DATA_DIR})
		# Override DATA_DIR by environement variable
		set(HYDROCHRONO_DATA_DIR $ENV{HYDROCHRONO_DATA_DIR})
	else()
		set(HYDROCHRONO_DATA_DIR "${PROJECT_SOURCE_DIR}/demos")
	endif()

	include(CTest)
	enable_testing()
    
    set(CHRONO_DLL_DIR "${Chrono_DIR}/../bin/Release")
    set(IRRLICHT_DLL_DIR "C:/libs/irrlicht-1.8.4/bin/Win64-VisualStudio")
    set(TEST_ENVIRONMENT "PATH=${CHRONO_DLL_DIR};${IRRLICHT_DLL_DIR};$ENV{PATH}")

	add_subdirectory(tests)
	add_subdirectory(tests/regression)

endif(HYDROCHRONO_ENABLE_TESTS)



# Not a good idea to copy DLLs
# dosen't work on Linux
#set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
#add_DLL_copy_command("${CHRONO_DLLS}")

include(GNUInstallDirs)

# Export to include as external library in other projects

install(TARGETS HydroChrono
    EXPORT HydroChronoTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT HydroChronoTargets
    FILE HydroChronoTargets.cmake
    DESTINATION "${CMAKE_INSTALL_DATADIR}/HydroChrono/cmake"
    NAMESPACE HydroChrono::
)

install(
    DIRECTORY include/hydroc
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/HydroChronoConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/HydroChronoConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/HydroChrono/cmake"
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/HydroChronoConfig.cmake"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/HydroChrono/cmake"
)

export(
    EXPORT HydroChronoTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/HydroChronoTargets.cmake"
    NAMESPACE HydroChrono::
)

# Set build type to Release but enable debug symbols
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi /O2")
