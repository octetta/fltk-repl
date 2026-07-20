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
option(REPL_BUILD_DEMO "Build the example demo application" ON)

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

message(STATUS "fltk-repl: configured. Build with: cmake --build <builddir> -j")
