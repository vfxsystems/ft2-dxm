cmake_minimum_required(VERSION 3.7)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_DEBUG "-g")

project(ft2-dxm LANGUAGES C CXX)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy compatibility for bundled subprojects" FORCE)

option(EXTERNAL_LIBFLAC "use external(system) flac library" OFF)
option(FT2_ENABLE_VENDOR_TESTS "Enable tests registered by bundled third-party subprojects" OFF)
option(FT2_ENABLE_VECTOR_UI "Enable selectable high-DPI/vector UI renderer backend contract" ON)
option(FT2_ENABLE_TTF "Enable optional SDL_ttf-backed scalable UI font rendering" OFF)

# ----- Dexed FM synth integration (optional) -----
option(WITH_DEXED "Build with embedded Dexed FM synth" ON)

if(BUILD_TESTING AND NOT FT2_ENABLE_VENDOR_TESTS)
    set(_FT2_TOP_LEVEL_BUILD_TESTING "${BUILD_TESTING}")
    set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)
endif()

add_subdirectory(src/gearmulator/source/dsp56300/source EXCLUDE_FROM_ALL)
add_subdirectory(src/gearmulator/source/baseLib EXCLUDE_FROM_ALL)
add_subdirectory(src/gearmulator/source/libresample EXCLUDE_FROM_ALL)
add_subdirectory(src/gearmulator/source/synthLib EXCLUDE_FROM_ALL)
add_subdirectory(src/gearmulator/source/virusLib EXCLUDE_FROM_ALL)
add_subdirectory(src/gearmulator/source/dsp56300/source/wxWidgets/src/zlib EXCLUDE_FROM_ALL)

if(DEFINED _FT2_TOP_LEVEL_BUILD_TESTING)
    set(BUILD_TESTING "${_FT2_TOP_LEVEL_BUILD_TESTING}" CACHE BOOL "Build tests" FORCE)
endif()

find_package(SDL2 REQUIRED)
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# On non-Windows we want the *system* SDL2 to win over the bundled src/SDL2/
# headers (which exist for the MSVC build). Because src/ is on the -I path,
# `<SDL2/SDL.h>` would otherwise resolve to the bundled copy and mismatch the
# linked system libSDL2. Adding the system SDL2's parent dir BEFORE src/
# (computed from SDL2_INCLUDE_DIRS) ensures `<SDL2/...>` hits the system copy.
if(NOT WIN32)
    if(SDL2_INCLUDE_DIRS)
        list(GET SDL2_INCLUDE_DIRS 0 _ft2_sdl2_inc0)
    elseif(TARGET SDL2::SDL2)
        get_target_property(_ft2_sdl2_inc0 SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
    endif()
    if(_ft2_sdl2_inc0)
        get_filename_component(_ft2_sdl2_inc_parent "${_ft2_sdl2_inc0}" DIRECTORY)
        message(STATUS "ft2-dxm: SDL2 include parent = ${_ft2_sdl2_inc_parent}")
    endif()
endif()

file(GLOB ft2_dxm_SRC
    "${CMAKE_SOURCE_DIR}/src/rtmidi/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/shared/*.c"
    "${CMAKE_SOURCE_DIR}/src/gfxdata/*.c"
    "${CMAKE_SOURCE_DIR}/src/mixer/*.c"
    "${CMAKE_SOURCE_DIR}/src/scopes/*.c"
    "${CMAKE_SOURCE_DIR}/src/modloaders/*.c"
    "${CMAKE_SOURCE_DIR}/src/smploaders/*.c"
)


#

add_executable(ft2-dxm ${ft2_dxm_SRC})

# FT2 SDL drawing integration (no external GUI toolkit needed)
target_include_directories(ft2-dxm SYSTEM
    PRIVATE ${SDL2_INCLUDE_DIRS})

target_include_directories(ft2-dxm PRIVATE
    ${CMAKE_SOURCE_DIR}/src)

if(NOT WIN32 AND _ft2_sdl2_inc_parent)
    # CMake silently drops implicit system include paths (like /usr/include)
    # from target_include_directories. Stage a build-dir alias that just
    # re-exports the system SDL2 headers, then put that on the include path
    # ahead of src/ so `<SDL2/SDL.h>` resolves to the system headers rather
    # than the bundled src/SDL2/ (which targets a newer SDL2 than the libs
    # actually linked on Linux distros like Ubuntu 22.04).
    set(_ft2_sdl2_shim "${CMAKE_BINARY_DIR}/_ft2_sdl2_shim")
    file(MAKE_DIRECTORY "${_ft2_sdl2_shim}/SDL2")
    file(GLOB _ft2_sdl2_sys_hdrs "${_ft2_sdl2_inc0}/*.h")
    foreach(_h ${_ft2_sdl2_sys_hdrs})
        get_filename_component(_hn "${_h}" NAME)
        configure_file("${_h}" "${_ft2_sdl2_shim}/SDL2/${_hn}" COPYONLY)
    endforeach()
    target_include_directories(ft2-dxm BEFORE PRIVATE "${_ft2_sdl2_shim}")
endif()

if("${SDL2_LIBRARIES}" STREQUAL "")
    message(WARNING "SDL2_LIBRARIES wasn't set, manually setting to SDL2::SDL2")
    set(SDL2_LIBRARIES "SDL2::SDL2")
endif()

find_package(Threads REQUIRED)

if(NOT WIN32)
    target_link_libraries(ft2-dxm PRIVATE m)
else()
    target_link_libraries(ft2-dxm PRIVATE shlwapi winmm)
endif()

target_link_libraries(ft2-dxm
    PRIVATE Threads::Threads ${SDL2_LIBRARIES})

if(FT2_ENABLE_TTF)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2TTF QUIET IMPORTED_TARGET SDL2_ttf)
    if(NOT SDL2TTF_FOUND)
        message(FATAL_ERROR "FT2_ENABLE_TTF requires the SDL2_ttf development package (Debian/Ubuntu: libsdl2-ttf-dev, Fedora: SDL2_ttf-devel, Arch: sdl2_ttf).")
    endif()
    target_compile_definitions(ft2-dxm PRIVATE FT2_ENABLE_TTF)
    target_link_libraries(ft2-dxm PRIVATE PkgConfig::SDL2TTF)
endif()

if(FT2_ENABLE_VECTOR_UI)
    target_compile_definitions(ft2-dxm PRIVATE FT2_ENABLE_VECTOR_UI)
endif()

target_compile_definitions(ft2-dxm
    PRIVATE HAS_MIDI
    PRIVATE HAS_LIBFLAC)

if(MSVC)
    target_compile_definitions(ft2-dxm PRIVATE _USE_MATH_DEFINES)
endif()

if(UNIX)
    if(APPLE)
        find_library(COREAUDIO CoreAudio REQUIRED)
        find_library(COREFOUNDATION CoreFoundation REQUIRED)
        find_library(COREMIDI CoreMIDI REQUIRED)
        find_library(ICONV iconv REQUIRED)
        target_link_libraries(ft2-dxm
            PRIVATE ${COREAUDIO} ${COREMIDI} ${COREFOUNDATION} ${ICONV})
        target_compile_definitions(ft2-dxm
            PRIVATE __MACOSX_CORE__)
    else()
        target_link_libraries(ft2-dxm
            PRIVATE asound)
        target_compile_definitions(ft2-dxm
            PRIVATE __LINUX_ALSA__)
    endif()
endif()

if(EXTERNAL_LIBFLAC)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac)
    target_compile_definitions(ft2-dxm
        PRIVATE EXTERNAL_LIBFLAC)
    target_link_libraries(ft2-dxm
        PRIVATE PkgConfig::FLAC)
else()
    file(GLOB flac_SRCS
        "${CMAKE_SOURCE_DIR}/src/libflac/*.c")
    target_sources(ft2-dxm PRIVATE ${flac_SRCS})
endif()

if(WITH_DEXED)
    # Dexed embedded DSP sources (JUCE-free)
    file(GLOB DEXED_FULL_MSFA_SRCS
        "${CMAKE_SOURCE_DIR}/src/dexed/msfa/*.cc"
        "${CMAKE_SOURCE_DIR}/src/dexed/msfa/*.cpp"
    )
    set(DEXED_CORE_SRCS
        ${CMAKE_SOURCE_DIR}/src/dexed/EngineMkI.cpp
        ${CMAKE_SOURCE_DIR}/src/dexed/EngineOpl.cpp
        ${CMAKE_SOURCE_DIR}/src/dexed/PluginData.cpp
        ${CMAKE_SOURCE_DIR}/src/dexed/PluginParam.cpp
        ${CMAKE_SOURCE_DIR}/src/dexed/dexed_audio.cpp
        ${CMAKE_SOURCE_DIR}/src/dexed/PluginFx.cpp
    )
    target_sources(ft2-dxm PRIVATE ${DEXED_FULL_MSFA_SRCS} ${DEXED_CORE_SRCS})

    # FT2 Dexed integration wrapper (includes DexedAudioProcessor via unity include)
    target_sources(ft2-dxm PRIVATE src/ft2_dexed_wrapper.cpp src/ft2_dexed.c src/ft2_dexed_wrapper_extra.cpp)

    target_include_directories(ft2-dxm PRIVATE
        ${CMAKE_SOURCE_DIR}/src/dexed
        ${CMAKE_SOURCE_DIR}/src/dexed/msfa
    )
    target_compile_definitions(ft2-dxm PRIVATE HAS_DEXED)

    set(DEXED_BUILTIN_ZIP "${CMAKE_SOURCE_DIR}/src/dexed/builtin_pgm.zip")
    if(EXISTS "${DEXED_BUILTIN_ZIP}")
        set(DEXED_EMBED_SRC "${CMAKE_BINARY_DIR}/generated/ft2_dexed_builtin_pgm_zip.cpp")
        add_custom_command(
            OUTPUT "${DEXED_EMBED_SRC}"
            COMMAND ${CMAKE_COMMAND}
                    -DINPUT=${DEXED_BUILTIN_ZIP}
                    -DOUTPUT=${DEXED_EMBED_SRC}
                    -DSYMBOL=ft2_dexed_builtin_pgm_zip
                    -P "${CMAKE_SOURCE_DIR}/cmake/embed_binary.cmake"
            DEPENDS "${DEXED_BUILTIN_ZIP}" "${CMAKE_SOURCE_DIR}/cmake/embed_binary.cmake"
            VERBATIM)
        target_sources(ft2-dxm PRIVATE "${DEXED_EMBED_SRC}")
        target_compile_definitions(ft2-dxm PRIVATE FT2_DEXED_EMBEDDED_BUILTIN_ZIP)

        add_custom_command(TARGET ft2-dxm POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ft2-dxm>/Dexed"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DEXED_BUILTIN_ZIP}"
                    "$<TARGET_FILE_DIR:ft2-dxm>/Dexed/builtin_pgm.zip"
            VERBATIM)
    endif()
endif()

target_include_directories(ft2-dxm PRIVATE
    ${CMAKE_SOURCE_DIR}/src/gearmulator/source/dsp56300/source/wxWidgets/src/zlib
    ${CMAKE_BINARY_DIR}/src/gearmulator/source/dsp56300/source/wxWidgets/src/zlib
)

target_sources(ft2-dxm PRIVATE src/ft2_ostirus_wrapper.cpp)
target_link_libraries(ft2-dxm PRIVATE virusLib zlibstatic)

set(OSTIRUS_ASSET_ZIP "${CMAKE_SOURCE_DIR}/src/gearmulator/assets/otstirusdat.zip")
if(EXISTS "${OSTIRUS_ASSET_ZIP}")
    set(OSTIRUS_EMBED_SRC "${CMAKE_BINARY_DIR}/generated/ft2_ostirusdat_zip.cpp")
    add_custom_command(
        OUTPUT "${OSTIRUS_EMBED_SRC}"
        COMMAND ${CMAKE_COMMAND}
                -DINPUT=${OSTIRUS_ASSET_ZIP}
                -DOUTPUT=${OSTIRUS_EMBED_SRC}
                -DSYMBOL=ft2_ostirusdat_zip
                -P "${CMAKE_SOURCE_DIR}/cmake/embed_binary.cmake"
        DEPENDS "${OSTIRUS_ASSET_ZIP}" "${CMAKE_SOURCE_DIR}/cmake/embed_binary.cmake"
        VERBATIM)
    target_sources(ft2-dxm PRIVATE "${OSTIRUS_EMBED_SRC}")
    target_compile_definitions(ft2-dxm PRIVATE FT2_OSTIRUS_EMBEDDED_ZIP)

    add_custom_command(TARGET ft2-dxm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ft2-dxm>/OsTIrus"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OSTIRUS_ASSET_ZIP}"
                "$<TARGET_FILE_DIR:ft2-dxm>/OsTIrus/otstirusdat.zip"
        VERBATIM)
endif()

install(TARGETS ft2-dxm
    RUNTIME DESTINATION bin)

# Tunefish4 synth integration: compile raw Tunefish4 engine into ft2-dxm
file(GLOB TUNEFISH_SRCS
    "${CMAKE_SOURCE_DIR}/src/tunefish4/Source/synth/tf4.cpp"
    "${CMAKE_SOURCE_DIR}/src/tunefish4/Source/synth/tf4fx.cpp"
    "${CMAKE_SOURCE_DIR}/src/tunefish4/Source/runtime/*.cpp"
)
target_sources(ft2-dxm PRIVATE ${TUNEFISH_SRCS})

# Add C++ wrapper for synth integration
target_sources(ft2-dxm PRIVATE src/ft2_synth_wrapper.cpp)

# Add V2 synth integration
target_sources(ft2-dxm PRIVATE
    src/ft2_v2_wrapper.cpp
    src/v2/v2defs.cpp
    src/v2/synth_core.cpp
)

# Add FT2 SDL drawing integration files
# Note: ft2_guisan_integration.c was removed in favor of direct SDL integration

target_include_directories(ft2-dxm PRIVATE
    ${CMAKE_SOURCE_DIR}/src/tunefish4/Source
    ${CMAKE_SOURCE_DIR}/src/tunefish4/Source/synth
)
# end of Tunefish4 integration

# Envelope UI integration: shared envelope editor code
target_sources(ft2-dxm PRIVATE src/envelope_ui.c)

# Tunefish modern widget system
target_sources(ft2-dxm PRIVATE
    src/ft2_tunefish_widgets.c
    src/ft2_tunefish_complete_layout.c
    src/ft2_tunefish_complete_layout_schema.c
    src/ft2_v2_complete_layout.c
    src/ft2_v2_complete_layout_schema.c
    src/ft2_ostirus_complete_layout.c
    src/ft2_ostirus_complete_layout_schema.c
    src/ft2_mixer_layout_schema.c
)
target_compile_definitions(ft2-dxm PRIVATE TF_USE_SCHEMA_LAYOUT)
# Dexed C UI layout (optional): include the lightweight C-based Dexed SDL editor
# when present so linker can resolve dx_* symbols used by the guisan integration.
target_sources(ft2-dxm PRIVATE
    src/dexed/dx_complete_layout.c
    src/dexed/dx_complete_layout_schema.c
)
target_compile_definitions(ft2-dxm PRIVATE DX_USE_SCHEMA_LAYOUT)

# Waveform view system
target_sources(ft2-dxm PRIVATE src/ft2_waveform_view.c)

# Macro map name table
target_sources(ft2-dxm PRIVATE src/ft2_macro_map.c)

# Generic popup list widget
target_sources(ft2-dxm PRIVATE src/ft2_popup_list.c)

# Render selection to sample slot implementation
target_sources(ft2-dxm PRIVATE src/ft2_slot_renderer.c)

# DXI (DXM Instrument) standalone synth patch file save/load
target_sources(ft2-dxm PRIVATE src/ft2_dxi.c)

if(BUILD_TESTING)
    add_test(NAME ft2_version COMMAND ft2-dxm --version)
    add_test(NAME ft2_self_test COMMAND ft2-dxm --self-test)
    if(FT2_ENABLE_VECTOR_UI)
        add_test(NAME ft2_self_test_vector_theme COMMAND ft2-dxm --self-test)
        set_tests_properties(ft2_self_test_vector_theme PROPERTIES
            ENVIRONMENT "FT2_UI_BACKEND=vector;FT2_UI_THEME=renoise")
    endif()
    add_test(NAME ft2_v2_stress COMMAND ft2-dxm --v2-stress-test)
endif()
