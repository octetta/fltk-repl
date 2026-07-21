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
# FLTK: fetched automatically, no manual submodule checkout required.
# See cmake/FetchFLTK.cmake for the resolution order / offline options.
# ---------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchFLTK.cmake)

# FLTK's own CMake exports differ a bit by version/branch: newer trees
# use namespaced ALIAS targets (fltk::fltk), older ones export plain
# targets (fltk). Resolve to whichever exists so this keeps working
# across FLTK releases.
if(TARGET fltk::fltk)
    set(REPL_FLTK_TARGET fltk::fltk)
elseif(TARGET fltk)
    set(REPL_FLTK_TARGET fltk)
else()
    message(FATAL_ERROR "fltk-repl: could not find an FLTK CMake target after fetch/add_subdirectory")
endif()

# ---------------------------------------------------------------------
# The REPL library itself. This is what you link against from your own
# CMake project if you don't want to build the demo.
# ---------------------------------------------------------------------
add_library(replfltk STATIC
    src/repl_api.cpp
    src/TerminalView.cpp
    src/TerminalView.h
    src/Theme.cpp
    src/Theme.h
    src/Tokenize.cpp
    src/Tokenize.h
    src/bitmap_win.h
    src/bitmap_win.cpp
    src/panel_dsl.h
    src/panel_dsl.cpp
)

target_include_directories(replfltk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        src
)

target_link_libraries(replfltk PUBLIC ${REPL_FLTK_TARGET})

# Belt-and-suspenders: FLTK 1.3.x's CMake build doesn't set
# INTERFACE_INCLUDE_DIRECTORIES on its exported target, so headers
# like FL/Fl.H won't be found via target_link_libraries() alone.
# cmake/FetchFLTK.cmake sets REPL_FLTK_INCLUDE_DIRS to the right paths
# regardless of which acquisition method was used. Newer FLTK (1.4+)
# trees that already export this correctly just get a harmless
# duplicate include path here.
if(REPL_FLTK_INCLUDE_DIRS)
    target_include_directories(replfltk PUBLIC ${REPL_FLTK_INCLUDE_DIRS})
endif()

if(APPLE)
    target_link_libraries(replfltk PUBLIC "-framework Cocoa")
endif()

# ---------------------------------------------------------------------
# Demo executable. Written as plain C (examples/demo/main.c) against
# the pure-C repl_api.h header -- no C++ knowledge required to read or
# extend it. It still links through the C++ toolchain because it pulls
# in the C++ FLTK backend transitively.
# ---------------------------------------------------------------------
option(REPL_BUILD_DEMO "Build the generic example application" OFF)

if(REPL_BUILD_DEMO)
    add_executable(repl_demo examples/demo/main.c)
    target_link_libraries(repl_demo PRIVATE replfltk)
    # main.c is C, but the final link needs the C++ runtime (FLTK is
    # C++). Telling CMake the linker language is CXX handles this
    # automatically on every platform/compiler this project targets.
    set_target_properties(repl_demo PROPERTIES LINKER_LANGUAGE CXX)

    if(WIN32)
        set_target_properties(repl_demo PROPERTIES WIN32_EXECUTABLE FALSE)
    endif()
endif()

# ---------------------------------------------------------------------
# Skred executable. It deliberately links against a packaged PULP
# release, not a library from PULP's build directory.
# ---------------------------------------------------------------------
option(SKRED_BUILD_REPL "Build the Skred REPL application" ON)
set(SKRED_ROOT "" CACHE PATH
    "Packaged Skred release root (contains include/skred and lib64 or lib)")
set(PULP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../pulp" CACHE PATH
    "PULP checkout used only to locate packaged releases under dist/")

if(SKRED_BUILD_REPL)
    if(NOT SKRED_ROOT AND DEFINED ENV{SKRED_ROOT})
        set(SKRED_ROOT "$ENV{SKRED_ROOT}")
    endif()

    if(NOT SKRED_ROOT)
        if(WIN32)
            set(_skred_platform "windows-x86_64")
        elseif(APPLE)
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
                set(_skred_platform "macos-arm64")
            else()
                set(_skred_platform "macos-x86_64")
            endif()
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
            set(_skred_platform "linux-x86_64")
        else()
            set(_skred_platform "linux-${CMAKE_SYSTEM_PROCESSOR}")
        endif()

        file(GLOB _skred_releases
            LIST_DIRECTORIES true
            "${PULP_ROOT}/dist/${_skred_platform}/skred-*-maxed")
        set(_skred_best_version "0")
        foreach(_candidate IN LISTS _skred_releases)
            get_filename_component(_name "${_candidate}" NAME)
            if(_name MATCHES "^skred-([0-9]+\\.[0-9]+\\.[0-9]+)-maxed$")
                set(_version "${CMAKE_MATCH_1}")
                if(_version VERSION_GREATER _skred_best_version)
                    set(_skred_best_version "${_version}")
                    set(SKRED_ROOT "${_candidate}")
                endif()
            endif()
        endforeach()
    endif()

    find_path(SKRED_INCLUDE_DIR
        NAMES skred/api.h
        HINTS "${SKRED_ROOT}/include"
        NO_DEFAULT_PATH)
    find_library(SKRED_API_LIBRARY
        NAMES api
        HINTS "${SKRED_ROOT}/lib64" "${SKRED_ROOT}/lib"
        NO_DEFAULT_PATH)

    if(NOT SKRED_INCLUDE_DIR OR NOT SKRED_API_LIBRARY)
        message(FATAL_ERROR
            "No packaged Skred release API was found.\n"
            "Set -DSKRED_ROOT=/path/to/skred-<version>-maxed; that directory\n"
            "must contain include/skred/api.h and lib64/libapi (or lib/libapi).")
    endif()

    add_executable(skred_repl examples/skred/main.c)
    target_include_directories(skred_repl PRIVATE "${SKRED_INCLUDE_DIR}")
    target_link_libraries(skred_repl PRIVATE replfltk "${SKRED_API_LIBRARY}")
    set_target_properties(skred_repl PROPERTIES
        LINKER_LANGUAGE CXX
        BUILD_RPATH "${SKRED_ROOT}/lib64;${SKRED_ROOT}/lib")

    if(WIN32)
        set_target_properties(skred_repl PROPERTIES WIN32_EXECUTABLE FALSE)
    endif()

    message(STATUS "skred-repl: release API: ${SKRED_ROOT}")
endif()

message(STATUS "skred-repl: configured. Build with: cmake --build <builddir> -j")
