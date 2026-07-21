cmake_minimum_required(VERSION 3.16)
project(fltk_repl LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------
# FLTK
# ---------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchFLTK.cmake)

if(NOT FLTK_VERSION OR FLTK_VERSION VERSION_LESS "1.4.0")
    message(FATAL_ERROR "fltk-repl: FLTK 1.4.0 or newer is required (found '${FLTK_VERSION}')")
endif()

if(NOT TARGET fltk::fltk OR NOT TARGET fltk::images)
    message(FATAL_ERROR "fltk-repl: FLTK core and image targets are required")
endif()

set(REPL_FLTK_TARGET fltk::images)
message(STATUS "fltk-repl: FLTK ${FLTK_VERSION} with SVG support")

# ---------------------------------------------------------------------
# REPL library
# ---------------------------------------------------------------------
add_library(replfltk STATIC
    src/repl_api.cpp src/TerminalView.cpp src/TerminalView.h
    src/Theme.cpp src/Theme.h
    src/Tokenize.cpp src/Tokenize.h
    src/bitmap_win.cpp src/panel_dsl.cpp
    include/repl/bitmap_win.h include/repl/panel_dsl.h
)

target_include_directories(replfltk PUBLIC include PRIVATE src)
target_link_libraries(replfltk PUBLIC ${REPL_FLTK_TARGET})

if(APPLE)
    target_link_libraries(replfltk PUBLIC "-framework Cocoa")
endif()

# ---------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------
if(WIN32)
    set(_skred_platform "windows-x86_64")
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        set(_skred_platform "macos-arm64")
    else()
        set(_skred_platform "macos-x86_64")
    endif()
else()
    set(_skred_platform "linux-x86_64")
endif()

# ---------------------------------------------------------------------
# Download Skred release into vendor/
# ---------------------------------------------------------------------
option(SKRED_BUILD_REPL "Build Skred REPL" ON)
set(SKRED_VERSION "0.51.0" CACHE STRING "Skred version to use")
set(SKRED_ROOT "" CACHE PATH "Path to skred package (auto-filled)")

set(VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
set(SKRED_DIR "${VENDOR_DIR}/skred-${SKRED_VERSION}-maxed")

if(SKRED_BUILD_REPL)
    if(NOT EXISTS "${SKRED_DIR}/include/skred/api.h")
        message(STATUS "Downloading Skred ${SKRED_VERSION} for ${_skred_platform}...")

        file(MAKE_DIRECTORY "${VENDOR_DIR}")
        set(_url "https://github.com/octetta/pulp/releases/download/v${SKRED_VERSION}/skred-${SKRED_VERSION}-maxed-${_skred_platform}.tar.gz")

        execute_process(
            COMMAND curl -L -o skred.tar.gz "${_url}"
            WORKING_DIRECTORY "${VENDOR_DIR}"
            RESULT_VARIABLE DOWNLOAD_RESULT
        )

        if(NOT DOWNLOAD_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to download Skred from ${_url}\nCheck the release page: https://github.com/octetta/pulp/releases")
        endif()

        execute_process(COMMAND tar -xzf skred.tar.gz WORKING_DIRECTORY "${VENDOR_DIR}")
        file(REMOVE "${VENDOR_DIR}/skred.tar.gz")
    endif()

    set(SKRED_ROOT "${SKRED_DIR}")
    message(STATUS "Using downloaded Skred: ${SKRED_ROOT}")

    find_path(SKRED_INCLUDE_DIR NAMES skred/api.h HINTS "${SKRED_ROOT}/include" NO_DEFAULT_PATH)
    find_library(SKRED_API_LIBRARY NAMES api HINTS "${SKRED_ROOT}/lib64" "${SKRED_ROOT}/lib" NO_DEFAULT_PATH)

    if(NOT SKRED_INCLUDE_DIR OR NOT SKRED_API_LIBRARY)
        message(FATAL_ERROR "Downloaded Skred is incomplete")
    endif()

    add_executable(skred_repl examples/skred/main.c)
    target_include_directories(skred_repl PRIVATE ${SKRED_INCLUDE_DIR})
    target_link_libraries(skred_repl PRIVATE replfltk ${SKRED_API_LIBRARY})
    set_target_properties(skred_repl PROPERTIES
        LINKER_LANGUAGE CXX
        BUILD_RPATH "${SKRED_ROOT}/lib64;${SKRED_ROOT}/lib"
    )

    if(WIN32)
        set_target_properties(skred_repl PROPERTIES WIN32_EXECUTABLE FALSE)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(repl_svg_smoke tests/svg_smoke.cpp)
    target_link_libraries(repl_svg_smoke PRIVATE replfltk)
    add_test(NAME repl_svg_smoke COMMAND repl_svg_smoke)
endif()

message(STATUS "fltk-repl configured successfully!")
