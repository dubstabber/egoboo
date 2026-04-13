function(egoboo_stage_windows_runtime_libraries target dependency_target)
    if (NOT WIN32)
        return()
    endif()

    get_property(_runtime_libraries TARGET ${dependency_target} PROPERTY runtime-libraries)
    foreach(_runtime_library ${_runtime_libraries})
        if (EXISTS "${_runtime_library}")
            get_filename_component(_runtime_name "${_runtime_library}" NAME)
            add_custom_command(
                TARGET ${target}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_runtime_library}"
                        "$<TARGET_FILE_DIR:${target}>/${_runtime_name}")
            install(FILES "${_runtime_library}"
                    DESTINATION bin
                    COMPONENT applications)
        endif()
    endforeach()
endfunction()
