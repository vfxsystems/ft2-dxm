include_guard(GLOBAL)

function(ft2_stage_sdl2_runtime target_name)
    if(NOT WIN32 OR NOT TARGET "${target_name}")
        return()
    endif()

    set(_ft2_sdl_targets SDL2::SDL2 SDL2::SDL2-shared)
    set(_ft2_sdl_properties
        IMPORTED_LOCATION
        IMPORTED_LOCATION_RELEASE
        IMPORTED_LOCATION_RELWITHDEBINFO
        IMPORTED_LOCATION_MINSIZEREL
        IMPORTED_LOCATION_DEBUG)
    set(_ft2_sdl_runtime_candidates)

    foreach(_ft2_sdl_target IN LISTS _ft2_sdl_targets)
        if(TARGET "${_ft2_sdl_target}")
            foreach(_ft2_sdl_property IN LISTS _ft2_sdl_properties)
                get_target_property(_ft2_sdl_location
                    "${_ft2_sdl_target}" "${_ft2_sdl_property}")
                if(_ft2_sdl_location AND
                   NOT _ft2_sdl_location MATCHES "-NOTFOUND$" AND
                   _ft2_sdl_location MATCHES "\\.[Dd][Ll][Ll]$")
                    list(APPEND _ft2_sdl_runtime_candidates "${_ft2_sdl_location}")
                endif()
            endforeach()
        endif()
    endforeach()

    if(DEFINED SDL2_DLL AND NOT "${SDL2_DLL}" STREQUAL "")
        list(APPEND _ft2_sdl_runtime_candidates "${SDL2_DLL}")
    elseif(DEFINED ENV{SDL2_DLL} AND NOT "$ENV{SDL2_DLL}" STREQUAL "")
        list(APPEND _ft2_sdl_runtime_candidates "$ENV{SDL2_DLL}")
    endif()
    if(DEFINED SDL2_DIR AND NOT "${SDL2_DIR}" STREQUAL "")
        get_filename_component(_ft2_sdl_prefix
            "${SDL2_DIR}/../../.." ABSOLUTE)
        list(APPEND _ft2_sdl_runtime_candidates
            "${_ft2_sdl_prefix}/bin/SDL2.dll")
    endif()

    foreach(_ft2_sdl_runtime IN LISTS _ft2_sdl_runtime_candidates)
        if(EXISTS "${_ft2_sdl_runtime}")
            add_custom_command(TARGET "${target_name}" POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_ft2_sdl_runtime}"
                    "$<TARGET_FILE_DIR:${target_name}>/SDL2.dll"
                VERBATIM)
            message(STATUS
                "${target_name}: staging SDL2 runtime from ${_ft2_sdl_runtime}")
            return()
        endif()
    endforeach()

    message(WARNING
        "${target_name}: SDL2.dll was not found for runtime staging; set SDL2_DLL or use an SDL2 CMake package that exposes the DLL location")
endfunction()
