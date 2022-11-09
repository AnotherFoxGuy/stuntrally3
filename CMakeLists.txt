#
# This assumes you used scripts to get and build latest Ogre-next from sources
# And your Ogre source code is above, in ../Ogre/ogre-next and ../Ogre/ogre-next-deps
# Tested on Linux.
# Windows most likely broken and it probably needs Ogre inside Dependencies subdir here.
# see CMake/DependenciesConfig.cmake
# Conan can be is used to get SR deps.
#

cmake_minimum_required(VERSION 3.16)

# Include path for additional CMake library finding scripts
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/" ${CMAKE_MODULE_PATH})

# Add a sensible build type default and warning because empty means no optimization and no debug info.
set(CMAKE_CONFIGURATION_TYPES "Debug" "Release" CACHE STRING "Configuration types")

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.22")
    cmake_policy(SET CMP0127 OLD)
endif ()

include(CMakeDependentOption)
include(Macros)
include(FeatureSummary)

project(StuntRally CXX C)

set(_PREFIX "SR_")

# Test if conan is installed
find_program(CONAN_CMD_TEST conan NO_CACHE)
cmake_dependent_option(USE_PACKAGE_MANAGER "Use conan" ON "NOT CONAN_CMD_TEST STREQUAL CONAN_CMD_TEST-NOTFOUND" OFF)
message(STATUS "Using conan: ${USE_PACKAGE_MANAGER}")

# Allow user to globally set the library preference for external libraries
cmd_option(${_PREFIX}LIB_PREFERENCE
        "Library preference [SYSTEM (if available), CONAN]"
        "CONAN"
        STRINGS "SYSTEM" "CONAN"
        )

# Special mode, that will force dependencies to the packages provided by system unless they were set to conan explicitly.
option(${_PREFIX}FORCE_SYSTEM_DEPENDENCIES "Force the use of system packages")

# Avoid source tree pollution
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

# Set default compile flags for GCC
if (CMAKE_COMPILER_IS_GNUCXX)
    message(STATUS "GCC detected, adding compile flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif ()
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")


include(CMake/Bootstrap.cmake)

# Check for dependencies
include(DependenciesConfig)
resolve_conan_dependencies()

set(CMAKE_CXX_STANDARD 17)


# Setup our application
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    if (NOT PLATFORM_X64)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
    endif ()
    add_definitions(-DUNICODE -D_UNICODE)
endif ()


macro(add_recursive dir retVal)
    file(GLOB_RECURSE ${retVal} ${dir}/*.h ${dir}/*.cpp ${dir}/*.c)
endmacro()


#  compiler  ----
include_directories(
        src
        src/btOgre2
        src/common
        src/common/data
        src/game
        src/road
        src/OgreCommon
        src/OgreCommon/System
        src/OgreCommon/System/Desktop
        src/OgreCommon/Threading
        src/OgreCommon/Utils
        src/Terra
        src/Terra/Hlms
        src/Terra/Hlms/PbsListener
        src/sound
        src/vdrift
)

add_recursive(./src GAME_SOURCES)

add_executable(${PROJECT_NAME} WIN32 ${GAME_SOURCES})

target_precompile_headers(${PROJECT_NAME} PRIVATE src/game/pch.h)

if (LINUX)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wswitch)
endif ()

message(STATUS "Libs: ${LIBS}")
target_link_libraries(${PROJECT_NAME} ${LIBS})
