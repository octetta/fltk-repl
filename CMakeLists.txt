cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" FLTK_REPL_VERSION
     LIMIT_COUNT 1)
if(NOT FLTK_REPL_VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic version (found '${FLTK_REPL_VERSION}')")
endif()
project(fltk_repl VERSION ${FLTK_REPL_VERSION} LANGUAGES C CXX)

string(TIMESTAMP FLTK_REPL_BUILD_DATE "%Y-%m-%dT%H:%M:%SZ" UTC)
set(PIKCHR_VERSION "1.0.0")

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
# Pikchr (vendored generated C source)
# ---------------------------------------------------------------------
add_library(pikchr STATIC third_party/pikchr/pikchr.c)
target_include_directories(pikchr PUBLIC third_party/pikchr)
if(NOT MSVC)
    target_link_libraries(pikchr PUBLIC m)
endif()

# ---------------------------------------------------------------------
# REPL library
# ---------------------------------------------------------------------
add_library(replfltk STATIC
    src/repl_api.cpp src/TerminalView.cpp src/TerminalView.h
    src/Theme.cpp src/Theme.h
    src/Tokenize.cpp src/Tokenize.h src/VectorFont.cpp src/VectorFont.h
    src/AudioMetrics.cpp src/AudioMetrics.h
    src/Spectrogram.cpp src/Spectrogram.h src/Waveform.cpp src/Waveform.h
    src/VoiceTopology.cpp src/VoiceTopology.h
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
    set(_skred_archive_extension "zip")
elseif(APPLE)
    set(_skred_platform "macos-universal")
    set(_skred_archive_extension "tar.gz")
else()
    set(_skred_platform "linux-x86_64")
    set(_skred_archive_extension "tar.gz")
endif()

# ---------------------------------------------------------------------
# Download Skred release into vendor/
# ---------------------------------------------------------------------
option(SKRED_BUILD_REPL "Build Skred REPL" ON)
set(SKRED_VERSION "0.52.0" CACHE STRING "Skred version to use")
set(SKRED_ROOT "" CACHE PATH "Path to skred package (auto-filled)")

set(VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
set(SKRED_DIR "${VENDOR_DIR}/skred-${SKRED_VERSION}-maxed")

if(SKRED_BUILD_REPL)
    if(NOT EXISTS "${SKRED_DIR}/include/skred/api.h")
        message(STATUS "Downloading Skred ${SKRED_VERSION} for ${_skred_platform}...")

        file(MAKE_DIRECTORY "${VENDOR_DIR}")
        set(_archive "skred-${SKRED_VERSION}-maxed-${_skred_platform}.${_skred_archive_extension}")
        set(_url "https://github.com/octetta/pulp/releases/download/v${SKRED_VERSION}/${_archive}")

        execute_process(
            COMMAND curl -fL -o "${_archive}" "${_url}"
            WORKING_DIRECTORY "${VENDOR_DIR}"
            RESULT_VARIABLE DOWNLOAD_RESULT
        )

        if(NOT DOWNLOAD_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to download Skred from ${_url}\nCheck the release page: https://github.com/octetta/pulp/releases")
        endif()

        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${_archive}"
            WORKING_DIRECTORY "${VENDOR_DIR}"
            RESULT_VARIABLE EXTRACT_RESULT
        )
        file(REMOVE "${VENDOR_DIR}/${_archive}")
        if(NOT EXTRACT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to extract ${_archive}")
        endif()
    endif()

    set(SKRED_ROOT "${SKRED_DIR}")
    message(STATUS "Using downloaded Skred: ${SKRED_ROOT}")

    find_path(SKRED_INCLUDE_DIR NAMES skred/api.h HINTS "${SKRED_ROOT}/include" NO_DEFAULT_PATH)
    find_library(SKRED_API_LIBRARY NAMES api HINTS "${SKRED_ROOT}/lib64" "${SKRED_ROOT}/lib" NO_DEFAULT_PATH)

    if(NOT SKRED_INCLUDE_DIR OR NOT SKRED_API_LIBRARY)
        message(FATAL_ERROR "Downloaded Skred is incomplete")
    endif()

    add_executable(skred_repl
        examples/skred/main.c
        examples/skred/SpectrogramBridge.cpp
        examples/skred/SpectrogramBridge.h
        examples/skred/TopologyWindow.cpp
        examples/skred/TopologyWindow.h
    )
    target_include_directories(skred_repl PRIVATE ${SKRED_INCLUDE_DIR} src)
    target_compile_definitions(skred_repl PRIVATE
        FLTK_REPL_VERSION="${PROJECT_VERSION}"
        FLTK_REPL_BUILD_DATE="${FLTK_REPL_BUILD_DATE}"
        FLTK_REPL_FLTK_VERSION="${FLTK_VERSION}"
        FLTK_REPL_PIKCHR_VERSION="${PIKCHR_VERSION}")
    target_link_libraries(skred_repl PRIVATE replfltk pikchr ${SKRED_API_LIBRARY})
    set_target_properties(skred_repl PROPERTIES
        LINKER_LANGUAGE CXX
        OUTPUT_NAME "skrepl"
        BUILD_RPATH "${SKRED_ROOT}/lib64;${SKRED_ROOT}/lib"
    )

    if(APPLE)
        option(FLTK_REPL_ADHOC_SIGN
               "Ad-hoc sign the macOS application bundle after building" ON)
        set_target_properties(skred_repl PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
            MACOSX_BUNDLE_BUNDLE_NAME "skrepl"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.octetta.skrepl"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            OUTPUT_NAME "skrepl"
            BUILD_RPATH "@executable_path/../Frameworks"
        )

        file(GLOB _skred_macos_dylibs "${SKRED_ROOT}/lib/*.dylib")
        if(NOT _skred_macos_dylibs)
            message(FATAL_ERROR "Skred macOS package contains no dylibs")
        endif()

        find_program(FLTK_REPL_CODESIGN_EXECUTABLE codesign)
        if(FLTK_REPL_ADHOC_SIGN AND NOT FLTK_REPL_CODESIGN_EXECUTABLE)
            message(FATAL_ERROR "codesign is required for FLTK_REPL_ADHOC_SIGN")
        endif()

        add_custom_command(TARGET skred_repl POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:skred_repl>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_skred_macos_dylibs}
                "$<TARGET_BUNDLE_DIR:skred_repl>/Contents/Frameworks"
            COMMENT "Bundling Skred libraries in skrepl.app"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        if(FLTK_REPL_ADHOC_SIGN)
            add_custom_command(TARGET skred_repl POST_BUILD
                COMMAND "${FLTK_REPL_CODESIGN_EXECUTABLE}"
                    --force --deep --sign - --timestamp=none
                    "$<TARGET_BUNDLE_DIR:skred_repl>"
                COMMENT "Ad-hoc signing skrepl.app"
                VERBATIM
            )
        endif()
    elseif(WIN32)
        set_target_properties(skred_repl PROPERTIES WIN32_EXECUTABLE FALSE)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(repl_svg_smoke tests/svg_smoke.cpp)
    target_link_libraries(repl_svg_smoke PRIVATE replfltk)
    add_test(NAME repl_svg_smoke COMMAND repl_svg_smoke)

    add_executable(repl_spectrogram_test tests/spectrogram_test.cpp)
    target_include_directories(repl_spectrogram_test PRIVATE src)
    target_link_libraries(repl_spectrogram_test PRIVATE replfltk)
    add_test(NAME repl_spectrogram_test COMMAND repl_spectrogram_test)

    add_executable(repl_waveform_test tests/waveform_test.cpp)
    target_include_directories(repl_waveform_test PRIVATE src)
    target_link_libraries(repl_waveform_test PRIVATE replfltk)
    add_test(NAME repl_waveform_test COMMAND repl_waveform_test)

    add_executable(repl_voice_topology_test tests/voice_topology_test.cpp)
    target_include_directories(repl_voice_topology_test PRIVATE src)
    target_link_libraries(repl_voice_topology_test PRIVATE replfltk pikchr)
    add_test(NAME repl_voice_topology_test COMMAND repl_voice_topology_test)

    add_executable(repl_vector_font_test tests/vector_font_test.cpp)
    target_include_directories(repl_vector_font_test PRIVATE src)
    target_link_libraries(repl_vector_font_test PRIVATE replfltk)
    add_test(NAME repl_vector_font_test COMMAND repl_vector_font_test)
endif()

message(STATUS "fltk-repl configured successfully!")
