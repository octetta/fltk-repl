# Acquire FLTK for the build without requiring the user to manually
# clone submodules or install anything system-wide first.
#
# Resolution order:
#   1. If REPL_FLTK_DIR is set (e.g. -DREPL_FLTK_DIR=/path/to/fltk-src),
#      use that local FLTK source tree via add_subdirectory().
#   2. Else if third_party/fltk already contains a CMakeLists.txt (the
#      user manually vendored a copy, or a previous configure step
#      downloaded one), use that.
#   3. Else use CMake's FetchContent to download FLTK from GitHub at
#      configure time. This is what "just works" for `cmake -B build`
#      on a fresh checkout with network access.
#
# Set REPL_FETCH_FLTK=OFF to disable step 3 (useful for fully offline/
# air-gapped builds where you pre-populate third_party/fltk yourself).

include(FetchContent)

set(REPL_FLTK_GIT_REPO "https://github.com/fltk/fltk.git" CACHE STRING
    "Git URL to fetch FLTK from when vendoring automatically")
set(REPL_FLTK_GIT_TAG "release-1.3.9" CACHE STRING
    "Git tag/branch of FLTK to fetch")
option(REPL_FETCH_FLTK "Automatically download FLTK via FetchContent if not found locally" ON)

set(_repl_local_fltk "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fltk")

if(REPL_FLTK_DIR)
    message(STATUS "fltk-repl: using FLTK from REPL_FLTK_DIR=${REPL_FLTK_DIR}")
    add_subdirectory(${REPL_FLTK_DIR} ${CMAKE_BINARY_DIR}/_fltk_build EXCLUDE_FROM_ALL)
    # FLTK 1.3.x's own CMakeLists.txt uses directory-scoped
    # include_directories() rather than target_include_directories(),
    # so those paths are NOT propagated to external targets that just
    # link against the `fltk` target. Set them explicitly ourselves.
    set(REPL_FLTK_INCLUDE_DIRS ${REPL_FLTK_DIR} ${CMAKE_BINARY_DIR}/_fltk_build)
elseif(EXISTS "${_repl_local_fltk}/CMakeLists.txt")
    message(STATUS "fltk-repl: using vendored FLTK found at ${_repl_local_fltk}")
    add_subdirectory(${_repl_local_fltk} ${CMAKE_BINARY_DIR}/_fltk_build EXCLUDE_FROM_ALL)
    set(REPL_FLTK_INCLUDE_DIRS ${_repl_local_fltk} ${CMAKE_BINARY_DIR}/_fltk_build)
elseif(REPL_FETCH_FLTK)
    message(STATUS "fltk-repl: fetching FLTK ${REPL_FLTK_GIT_TAG} from ${REPL_FLTK_GIT_REPO}")

    # Keep FLTK's own build small and fast -- we only need the core
    # widget + images (for icons/native file chooser) + no GL/test apps.
    set(FLTK_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(FLTK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FLTK_BUILD_FLUID OFF CACHE BOOL "" FORCE)
    set(OPTION_BUILD_HTML_DOCUMENTATION OFF CACHE BOOL "" FORCE)
    set(OPTION_BUILD_PDF_DOCUMENTATION OFF CACHE BOOL "" FORCE)
    set(FLTK_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        fltk
        GIT_REPOSITORY ${REPL_FLTK_GIT_REPO}
        GIT_TAG ${REPL_FLTK_GIT_TAG}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(fltk)

    # Same reasoning as the two branches above: FLTK 1.3.x doesn't set
    # INTERFACE_INCLUDE_DIRECTORIES on its `fltk` target, so we recover
    # the paths ourselves. FetchContent_MakeAvailable() populates
    # <name>_SOURCE_DIR / <name>_BINARY_DIR (lowercased) automatically.
    set(REPL_FLTK_INCLUDE_DIRS ${fltk_SOURCE_DIR} ${fltk_BINARY_DIR})
else()
    message(FATAL_ERROR
        "fltk-repl: no FLTK found and REPL_FETCH_FLTK is OFF. Either:\n"
        "  - set -DREPL_FLTK_DIR=/path/to/fltk-source, or\n"
        "  - place a copy of the FLTK source tree at third_party/fltk, or\n"
        "  - re-run with REPL_FETCH_FLTK=ON and network access.")
endif()
