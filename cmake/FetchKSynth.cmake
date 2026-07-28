# Acquire KSynth for the build without requiring the user to manually
# clone submodules or install anything system-wide first.
#
# Resolution order:
#   1. If KREPL_KSYNTH_DIR is set (e.g. -DKREPL_KSYNTH_DIR=/path/to/k-synth),
#      use that local source tree directly.
#   2. Else if third_party/ksynth already contains ksynth.h (a user manually
#      placed a copy there, or a previous configure step downloaded one),
#      use that.
#   3. Else use CMake's FetchContent to clone k-synth from GitHub at
#      configure time. This is what "just works" for `cmake -B build`
#      on a fresh checkout with network access.
#
# Set KREPL_FETCH_KSYNTH=OFF to disable step 3 (useful for fully offline/
# air-gapped builds where you pre-populate third_party/ksynth yourself).

include(FetchContent)

set(KREPL_KSYNTH_GIT_REPO "https://github.com/octetta/k-synth.git" CACHE STRING
    "Git URL to fetch KSynth from when vendoring automatically")
set(KREPL_KSYNTH_GIT_TAG "main" CACHE STRING
    "Git tag/branch of KSynth to fetch")
option(KREPL_FETCH_KSYNTH "Automatically download KSynth via FetchContent if not found locally" ON)

set(_krepl_local_ksynth "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ksynth")

if(KREPL_KSYNTH_DIR)
    message(STATUS "fltk-repl: using KSynth from KREPL_KSYNTH_DIR=${KREPL_KSYNTH_DIR}")
    set(KSYNTH_SOURCE_DIR "${KREPL_KSYNTH_DIR}")
elseif(EXISTS "${_krepl_local_ksynth}/ksynth.h")
    message(STATUS "fltk-repl: using vendored KSynth found at ${_krepl_local_ksynth}")
    set(KSYNTH_SOURCE_DIR "${_krepl_local_ksynth}")
elseif(KREPL_FETCH_KSYNTH)
    message(STATUS "fltk-repl: fetching KSynth ${KREPL_KSYNTH_GIT_TAG} from ${KREPL_KSYNTH_GIT_REPO}")
    FetchContent_Declare(
        ksynth_src
        GIT_REPOSITORY ${KREPL_KSYNTH_GIT_REPO}
        GIT_TAG        ${KREPL_KSYNTH_GIT_TAG}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(ksynth_src)
    set(KSYNTH_SOURCE_DIR "${ksynth_src_SOURCE_DIR}")
else()
    message(FATAL_ERROR
        "fltk-repl: no KSynth found and KREPL_FETCH_KSYNTH is OFF. Either:\n"
        "  - set -DKREPL_KSYNTH_DIR=/path/to/k-synth-source, or\n"
        "  - place a copy of the KSynth source at third_party/ksynth, or\n"
        "  - re-run with KREPL_FETCH_KSYNTH=ON and network access.")
endif()
