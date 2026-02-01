cmake_minimum_required(VERSION 3.7)

# Enable debugging symbols
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_DEBUG "-g")

project(ft2-dxm LANGUAGES C CXX)

option(EXTERNAL_LIBFLAC "use external(system) flac library" OFF)

# ----- Dexed FM synth integration (optional) -----
option(WITH_DEXED "Build with embedded Dexed FM synth" ON)

find_package(SDL2 REQUIRED)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/release/other/")

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

if("${SDL2_LIBRARIES}" STREQUAL "")
    message(WARNING "SDL2_LIBRARIES wasn't set, manually setting to SDL2::SDL2")
    set(SDL2_LIBRARIES "SDL2::SDL2")
endif()

find_package(Threads REQUIRED)

target_link_libraries(ft2-dxm
    PRIVATE m Threads::Threads ${SDL2_LIBRARIES})

target_compile_definitions(ft2-dxm
    PRIVATE HAS_MIDI
    PRIVATE HAS_LIBFLAC)

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

    # Builtin program ZIP (optional). We do not attempt to auto-extract here to avoid
    # using CMake features that may not be available on older CMake versions.
    set(DEXED_BUILTIN_ZIP "${CMAKE_SOURCE_DIR}/src/dexed/builtin_pgm.zip")
    if(EXISTS ${DEXED_BUILTIN_ZIP})
        message(STATUS "Found builtin Dexed program ZIP: ${DEXED_BUILTIN_ZIP} - automatic extraction disabled in this build.")
        message(STATUS "If you want CMake to extract it, upgrade to CMake 3.18+ or extract manually to ${CMAKE_BINARY_DIR}/dexed_builtin_extract or place .syx files under src/dexed/patches/")
    endif()
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
