cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" FLTK_REPL_VERSION
     LIMIT_COUNT 1)
if(NOT FLTK_REPL_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
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

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_link_options("-static" "-static-libgcc" "-static-libstdc++")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchFLTK.cmake)

if(NOT FLTK_VERSION OR FLTK_VERSION VERSION_LESS "1.4.0")
    message(FATAL_ERROR "fltk-repl: FLTK 1.4.0 or newer is required (found '${FLTK_VERSION}')")
endif()

if(NOT TARGET fltk::fltk OR NOT TARGET fltk::images)
    message(FATAL_ERROR "fltk-repl: FLTK core and image targets are required")
endif()

set(REPL_FLTK_TARGET fltk::images)
message(STATUS "fltk-repl: FLTK ${FLTK_VERSION} with SVG support")

add_library(pikchr STATIC third_party/pikchr/pikchr.c)
target_include_directories(pikchr PUBLIC third_party/pikchr)
if(NOT MSVC)
    target_link_libraries(pikchr PUBLIC m)
endif()

add_library(replfltk
    src/repl_api.cpp src/TerminalView.cpp src/TerminalView.h
    src/Theme.cpp src/Theme.h
    src/Tokenize.cpp src/Tokenize.h src/VectorFont.cpp src/VectorFont.h
    src/AudioMetrics.cpp src/AudioMetrics.h
    src/Spectrogram.cpp src/Spectrogram.h src/Waveform.cpp src/Waveform.h
    src/VoiceTopology.cpp src/VoiceTopology.h
    src/bitmap_win.cpp src/panel_dsl.cpp
    src/foreign_bridge.cpp src/EditorWindow.cpp src/EditorWindow.h
    src/UdpBridge.cpp src/UdpBridge.h
    src/FontPickerWindow.cpp src/FontPickerWindow.h
    include/repl/bitmap_win.h include/repl/panel_dsl.h
    include/repl/foreign_bridge.h include/repl/editor_win.h
    include/repl/udp_bridge.h include/repl/font_picker.h
    src/repl_prefs.cpp
)

add_library(fltk_repl::replfltk ALIAS replfltk)

target_include_directories(replfltk PUBLIC include PRIVATE src ${SKRED_INCLUDE_DIR})
target_link_libraries(replfltk PUBLIC ${REPL_FLTK_TARGET})

if(WIN32)
    target_link_libraries(replfltk PUBLIC ws2_32)
endif()

if(APPLE)
    target_link_libraries(replfltk PUBLIC "-framework Cocoa")
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(FLTK_REPL_IS_TOP_LEVEL ON)
else()
    set(FLTK_REPL_IS_TOP_LEVEL OFF)
endif()

option(REPL_BUILD_DEMO "Build REPL demo executable" ${FLTK_REPL_IS_TOP_LEVEL})
if(REPL_BUILD_DEMO)
    add_executable(repl_demo examples/demo/main.c)
    target_include_directories(repl_demo PRIVATE include)
    target_link_libraries(repl_demo PRIVATE replfltk)

    add_executable(repl_pipe_demo examples/pipe_demo/main.c)
    target_include_directories(repl_pipe_demo PRIVATE include)
    target_link_libraries(repl_pipe_demo PRIVATE replfltk)
endif()

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

option(SKRED_BUILD_REPL "Build Skred REPL" ${FLTK_REPL_IS_TOP_LEVEL})
set(SKRED_VERSION "0.55.9" CACHE STRING "Skred version to use")
set(SKRED_ROOT "" CACHE PATH "Path to skred package (auto-filled)")

set(VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
set(SKRED_DIR "${VENDOR_DIR}/skred-${SKRED_VERSION}-maxed")

if(SKRED_BUILD_REPL)
    if(NOT EXISTS "${SKRED_DIR}/include/skred/api.h")
        message(STATUS "Downloading Skred ${SKRED_VERSION} for ${_skred_platform}...")

        file(MAKE_DIRECTORY "${VENDOR_DIR}")
        set(_archive "skred-${SKRED_VERSION}-maxed-${_skred_platform}.${_skred_archive_extension}")
        set(SKRED_DOWNLOAD_OWNER "octetta" CACHE STRING "GitHub owner containing Skred release assets")
        set(SKRED_DOWNLOAD_REPO "pulp" CACHE STRING "GitHub repo containing Skred release assets")
        set(_url "https://github.com/${SKRED_DOWNLOAD_OWNER}/${SKRED_DOWNLOAD_REPO}/releases/download/v${SKRED_VERSION}/${_archive}")
        set(_archive_path "${VENDOR_DIR}/${_archive}")

        # Cross-platform download (no external curl binary required)
        file(DOWNLOAD "${_url}" "${_archive_path}"
            STATUS _dl_status
            SHOW_PROGRESS
            TLS_VERIFY ON
        )
        list(GET _dl_status 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            list(GET _dl_status 1 _dl_msg)
            message(FATAL_ERROR "Failed to download Skred from ${_url}: ${_dl_msg}")
        endif()

        option(SKRED_VERIFY_SHA256 "Verify Skred archive SHA256 after download" ON)
        set(SKRED_SHA256 "${SKRED_SHA256}" CACHE STRING "Expected SHA256 for Skred archive")

        if(SKRED_VERIFY_SHA256 AND SKRED_SHA256)
            file(SHA256 "${_archive_path}" _actual_sha256)
            string(TOLOWER "${_actual_sha256}" _actual_sha256)
            string(TOLOWER "${SKRED_SHA256}" _expect_sha256)
            if(NOT _actual_sha256 STREQUAL _expect_sha256)
                message(FATAL_ERROR "Skred archive checksum mismatch (expected ${_expect_sha256}, got ${_actual_sha256})")
            endif()
            message(STATUS "Skred archive SHA256 verified")
        endif()

        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${_archive}"
            WORKING_DIRECTORY "${VENDOR_DIR}"
            RESULT_VARIABLE EXTRACT_RESULT
        )
        file(REMOVE "${_archive_path}")
        if(NOT EXTRACT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to extract ${_archive}")
        endif()
    endif()

    if(EXISTS "${SKRED_DIR}/include/skred/api.h")
        set(SKRED_ROOT "${SKRED_DIR}")
    else()
        file(GLOB _skred_candidates
            "${VENDOR_DIR}/skred-${SKRED_VERSION}-maxed*"
            "${VENDOR_DIR}/skred-${SKRED_VERSION}-maxed*/*")
        set(SKRED_ROOT "")
        foreach(_cand IN LISTS _skred_candidates)
            if(EXISTS "${_cand}/include/skred/api.h")
                set(SKRED_ROOT "${_cand}")
                break()
            endif()
        endforeach()
        if(NOT SKRED_ROOT)
            message(FATAL_ERROR
                "Could not find skred/api.h under ${VENDOR_DIR} after download.")
        endif()
    endif()
    message(STATUS "Using downloaded Skred: ${SKRED_ROOT}")

    find_path(SKRED_INCLUDE_DIR NAMES skred/api.h HINTS "${SKRED_ROOT}/include" NO_DEFAULT_PATH)

    if(WIN32)
        if(EXISTS "${SKRED_ROOT}/lib/api.lib")
            set(SKRED_API_LIBRARY "${SKRED_ROOT}/lib/api.lib")
        elseif(EXISTS "${SKRED_ROOT}/lib/libapi.lib")
            set(SKRED_API_LIBRARY "${SKRED_ROOT}/lib/libapi.lib")
        elseif(EXISTS "${SKRED_ROOT}/lib/libapi.a")
            set(SKRED_API_LIBRARY "${SKRED_ROOT}/lib/libapi.a")
        elseif(EXISTS "${SKRED_ROOT}/lib/libapi.dll.a")
            set(SKRED_API_LIBRARY "${SKRED_ROOT}/lib/libapi.dll.a")
        else()
            set(SKRED_API_LIBRARY SKRED_API_LIBRARY-NOTFOUND)
        endif()
        if(SKRED_API_LIBRARY)
            message(STATUS "Skred: using ${SKRED_API_LIBRARY}")
        endif()
    else()
        find_library(SKRED_API_LIBRARY NAMES libapi.a
            HINTS "${SKRED_ROOT}/lib64" "${SKRED_ROOT}/lib" NO_DEFAULT_PATH)
    endif()

    if(NOT SKRED_INCLUDE_DIR OR NOT SKRED_API_LIBRARY)
        message(FATAL_ERROR "Downloaded Skred is incomplete (include=${SKRED_INCLUDE_DIR} lib=${SKRED_API_LIBRARY})")
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
    if(WIN32)
        target_link_libraries(skred_repl PRIVATE ws2_32 winmm ole32)
    elseif(APPLE)
        target_link_libraries(skred_repl PRIVATE "-framework CoreFoundation" "-framework CoreMIDI" "-framework CoreAudio")
    else()
        target_link_libraries(skred_repl PRIVATE asound)
    endif()
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
            MACOSX_BUNDLE_EXECUTABLE_NAME "skrepl"
            MACOSX_BUNDLE_BUNDLE_NAME "skrepl"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.octetta.skrepl"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            OUTPUT_NAME "skrepl"
            BUILD_RPATH "@executable_path/../Frameworks"
        )

        find_program(FLTK_REPL_CODESIGN_EXECUTABLE codesign)
        if(FLTK_REPL_ADHOC_SIGN AND NOT FLTK_REPL_CODESIGN_EXECUTABLE)
            message(FATAL_ERROR "codesign is required for FLTK_REPL_ADHOC_SIGN")
        endif()
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
        set_target_properties(skred_repl PROPERTIES WIN32_EXECUTABLE TRUE)
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

    add_executable(repl_panel_spring_test tests/panel_spring_test.cpp)
    target_include_directories(repl_panel_spring_test PRIVATE src include)
    target_link_libraries(repl_panel_spring_test PRIVATE replfltk)
    add_test(NAME repl_panel_spring_test COMMAND repl_panel_spring_test)

    add_executable(repl_udp_test tests/udp_test.cpp)
    target_include_directories(repl_udp_test PRIVATE src include)
    target_link_libraries(repl_udp_test PRIVATE replfltk)

    add_executable(repl_font_picker_test tests/font_picker_test.cpp)
    target_include_directories(repl_font_picker_test PRIVATE src include)
    target_link_libraries(repl_font_picker_test PRIVATE replfltk)

    add_executable(repl_panel_step_test tests/panel_step_test.cpp)
    target_include_directories(repl_panel_step_test PRIVATE src include)
    target_link_libraries(repl_panel_step_test PRIVATE replfltk)
    add_test(NAME repl_panel_step_test COMMAND repl_panel_step_test)
endif()

option(KREPL_BUILD "Build the krepl KSynth demo executable" OFF)
if(KREPL_BUILD)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchKSynth.cmake)

    add_library(ksynth STATIC
        "${KSYNTH_SOURCE_DIR}/ksynth.c"
        "${KSYNTH_SOURCE_DIR}/ks_api.c"
    )
    target_include_directories(ksynth PUBLIC "${KSYNTH_SOURCE_DIR}")

    add_executable(krepl examples/ksynth/main.c)
    target_include_directories(krepl PRIVATE "${KSYNTH_SOURCE_DIR}" src include)
    target_compile_definitions(krepl PRIVATE FLTK_REPL_VERSION="${PROJECT_VERSION}")
    target_link_libraries(krepl PRIVATE replfltk pikchr ksynth ${CMAKE_DL_LIBS})
    set_target_properties(krepl PROPERTIES LINKER_LANGUAGE CXX OUTPUT_NAME "krepl")
endif()

message(STATUS "fltk-repl configured successfully!")
