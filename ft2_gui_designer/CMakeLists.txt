cmake_minimum_required(VERSION 3.16)

project(ft2_gui_designer LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(DESIGNER_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(REPO_ROOT "${DESIGNER_ROOT}/..")

set(DESIGNER_SOURCES
    main.c
    palette.c
    canvas.c
    widgets.c
    font.c
    assets.c
    stb_image.c
    "${REPO_ROOT}/src/shared/ft2_ui_assets.c"
    "${REPO_ROOT}/src/shared/ft2_ui_widget_registry.c"
)

file(GLOB DESIGNER_GFX_SOURCES
    "${REPO_ROOT}/src/gfxdata/*.c")

list(APPEND DESIGNER_SOURCES ${DESIGNER_GFX_SOURCES})

add_executable(ft2_gui_designer ${DESIGNER_SOURCES})

target_include_directories(ft2_gui_designer PRIVATE
    "${DESIGNER_ROOT}"
    "${REPO_ROOT}/src"
)

if (WIN32)
    set(SDL2_ROOT "")
    if (EXISTS "${REPO_ROOT}/src/vs2019_project/ft2-clone/sdl")
        set(SDL2_ROOT "${REPO_ROOT}/src/vs2019_project/ft2-clone/sdl")
    elseif (EXISTS "${REPO_ROOT}/vs2019_project/ft2-clone/sdl")
        set(SDL2_ROOT "${REPO_ROOT}/vs2019_project/ft2-clone/sdl")
    endif()

    if (NOT SDL2_ROOT)
        message(FATAL_ERROR "Could not locate bundled SDL2 for ft2_gui_designer Windows build.")
    endif()

    set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include")
    set(SDL2_LIB_DIR "${SDL2_ROOT}/lib64")

    find_library(SDL2_LIBRARY
        NAMES SDL2
        PATHS "${SDL2_LIB_DIR}" "${SDL2_ROOT}/lib"
        NO_DEFAULT_PATH
        REQUIRED)

    find_library(SDL2MAIN_LIBRARY
        NAMES SDL2main
        PATHS "${SDL2_LIB_DIR}" "${SDL2_ROOT}/lib"
        NO_DEFAULT_PATH
        REQUIRED)

    add_library(SDL2::SDL2 UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}")

    add_library(SDL2::SDL2main UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2main PROPERTIES
        IMPORTED_LOCATION "${SDL2MAIN_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}")

    target_link_libraries(ft2_gui_designer PRIVATE SDL2::SDL2main SDL2::SDL2 opengl32 glu32)

    set(SDL2_RUNTIME_DLL "")
    if (EXISTS "${REPO_ROOT}/src/release/win64/SDL2.dll")
        set(SDL2_RUNTIME_DLL "${REPO_ROOT}/src/release/win64/SDL2.dll")
    elseif (EXISTS "${REPO_ROOT}/src/release/other/Release/SDL2.dll")
        set(SDL2_RUNTIME_DLL "${REPO_ROOT}/src/release/other/Release/SDL2.dll")
    elseif (EXISTS "${REPO_ROOT}/release/other/Release/SDL2.dll")
        set(SDL2_RUNTIME_DLL "${REPO_ROOT}/release/other/Release/SDL2.dll")
    endif()

    if (SDL2_RUNTIME_DLL)
        add_custom_command(TARGET ft2_gui_designer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SDL2_RUNTIME_DLL}"
                    "$<TARGET_FILE_DIR:ft2_gui_designer>/SDL2.dll"
            VERBATIM)
    endif()
else()
    find_package(SDL2 REQUIRED)
    target_include_directories(ft2_gui_designer SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(ft2_gui_designer PRIVATE ${SDL2_LIBRARIES} GL GLU m)
endif()
