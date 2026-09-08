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
    "${REPO_ROOT}/src/shared/ft2_ui_bitmap.c"
    "${REPO_ROOT}/src/shared/ft2_ui_widget_registry.c"
)

file(GLOB DESIGNER_GFX_SOURCES CONFIGURE_DEPENDS
    "${REPO_ROOT}/src/gfxdata/*.c")

list(APPEND DESIGNER_SOURCES ${DESIGNER_GFX_SOURCES})

add_executable(ft2_gui_designer ${DESIGNER_SOURCES})

add_custom_command(TARGET ft2_gui_designer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${DESIGNER_ROOT}/layouts"
            "$<TARGET_FILE_DIR:ft2_gui_designer>/layouts"
    VERBATIM)

target_include_directories(ft2_gui_designer PRIVATE
    "${DESIGNER_ROOT}"
    "${REPO_ROOT}/src"
)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

set(DESIGNER_SDL_MAIN_LIBRARY)
if (WIN32 AND TARGET SDL2::SDL2main)
    set(DESIGNER_SDL_MAIN_LIBRARY SDL2::SDL2main)
endif()
set(DESIGNER_SDL_LIBRARIES)
if (TARGET SDL2::SDL2)
    list(APPEND DESIGNER_SDL_LIBRARIES SDL2::SDL2)
elseif (SDL2_LIBRARIES)
    list(APPEND DESIGNER_SDL_LIBRARIES ${SDL2_LIBRARIES})
else()
    message(FATAL_ERROR "SDL2 was found but did not provide SDL2::SDL2 or SDL2_LIBRARIES")
endif()

if (SDL2_INCLUDE_DIRS)
    target_include_directories(ft2_gui_designer SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

set(DESIGNER_OPENGL_LIBRARIES OpenGL::GL)
if (TARGET OpenGL::GLU)
    list(APPEND DESIGNER_OPENGL_LIBRARIES OpenGL::GLU)
elseif (OPENGL_glu_LIBRARY)
    list(APPEND DESIGNER_OPENGL_LIBRARIES ${OPENGL_glu_LIBRARY})
else()
    message(FATAL_ERROR "FT2 GUI Designer requires GLU")
endif()

target_link_libraries(ft2_gui_designer PRIVATE
    ${DESIGNER_SDL_MAIN_LIBRARY}
    ${DESIGNER_SDL_LIBRARIES}
    ${DESIGNER_OPENGL_LIBRARIES})
if (UNIX AND NOT APPLE)
    target_link_libraries(ft2_gui_designer PRIVATE m)
endif()

include("${REPO_ROOT}/cmake/ft2_sdl2_runtime.cmake")
ft2_stage_sdl2_runtime(ft2_gui_designer)

install(TARGETS ft2_gui_designer RUNTIME DESTINATION bin)
install(DIRECTORY "${DESIGNER_ROOT}/layouts" DESTINATION bin)

include(CTest)
if (BUILD_TESTING)
    add_executable(ft2_gui_bitmap_tests
        bitmap_pipeline_tests.c
        palette.c
        assets.c
        stb_image.c
        "${REPO_ROOT}/src/shared/ft2_ui_assets.c"
        "${REPO_ROOT}/src/shared/ft2_ui_bitmap.c"
        ${DESIGNER_GFX_SOURCES})
    target_include_directories(ft2_gui_bitmap_tests PRIVATE
        "${DESIGNER_ROOT}"
        "${REPO_ROOT}/src")
    if (SDL2_INCLUDE_DIRS)
        target_include_directories(ft2_gui_bitmap_tests SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
    endif()
    target_link_libraries(ft2_gui_bitmap_tests PRIVATE ${DESIGNER_SDL_LIBRARIES})
    if (UNIX AND NOT APPLE)
        target_link_libraries(ft2_gui_bitmap_tests PRIVATE m)
    endif()

    add_test(NAME gui_designer_layout_validation
        COMMAND ft2_gui_designer --validate
            "${DESIGNER_ROOT}/test_design.gui"
            "layouts/tf_complete_layout.gui"
            "layouts/dx_complete_layout.gui"
            "layouts/ft2_mixer_layout.gui"
            "layouts/v2_complete_layout.gui"
            "layouts/ostirus_complete_layout.gui")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/invalid_layout.gui" "FT2GUI_V6\n1\ninvalid widget record\n")
    add_test(NAME gui_designer_rejects_malformed_layout
        COMMAND ft2_gui_designer --validate "${CMAKE_CURRENT_BINARY_DIR}/invalid_layout.gui")
    set_tests_properties(gui_designer_rejects_malformed_layout PROPERTIES WILL_FAIL TRUE)
    add_test(NAME gui_designer_bitmap_pipeline COMMAND ft2_gui_bitmap_tests)
endif()
