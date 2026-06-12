cmake_minimum_required(VERSION 3.24)

project(ResilientFileProtector
    VERSION 0.1.0
    DESCRIPTION "R.F.P. - Resilient File Protector"
    LANGUAGES CXX
)

option(RFP_BUILD_GUI "Build Qt GUI application" ON)
option(RFP_BUILD_CLI "Build command line utility" ON)
option(RFP_BUILD_TESTS "Build unit tests" ON)
option(RFP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(RFP_RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/build/bin"
    CACHE PATH
    "Directory for executable output files"
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${RFP_RUNTIME_OUTPUT_DIRECTORY}")
foreach(RFP_CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${RFP_CONFIG} "${RFP_RUNTIME_OUTPUT_DIRECTORY}")
endforeach()

include(ProjectOptions)
include(CompilerWarnings)

add_subdirectory(src/core)
add_subdirectory(src/stego)
add_subdirectory(src/crypto)

if(RFP_BUILD_CLI)
    add_subdirectory(src/cli)
endif()

if(RFP_BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

    # Use plain CMake automoc instead of newer Qt helper commands so the GUI
    # remains compatible with any installed Qt 6 release.
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    add_subdirectory(src/gui)
endif()

if(RFP_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()
