function(slm_require_project_paths)
    foreach(_rel IN LISTS ARGN)
        if(IS_ABSOLUTE "${_rel}")
            set(_path "${_rel}")
        else()
            set(_path "${CMAKE_SOURCE_DIR}/${_rel}")
        endif()

        if(NOT EXISTS "${_path}")
            message(FATAL_ERROR
                "Project structure invalid: required path missing:\n"
                "  ${_path}\n"
                "Fix: restore the file from git history before configuring.")
        endif()
    endforeach()
endfunction()
