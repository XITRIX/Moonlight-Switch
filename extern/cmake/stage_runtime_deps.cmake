cmake_minimum_required(VERSION 3.16)

if (NOT DEFINED MOONLIGHT_RUNTIME_EXECUTABLE OR
    MOONLIGHT_RUNTIME_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "MOONLIGHT_RUNTIME_EXECUTABLE is required")
endif ()

if (NOT DEFINED MOONLIGHT_RUNTIME_SEARCH_DIR OR
    MOONLIGHT_RUNTIME_SEARCH_DIR STREQUAL "")
    message(FATAL_ERROR "MOONLIGHT_RUNTIME_SEARCH_DIR is required")
endif ()

if (NOT DEFINED MOONLIGHT_OBJDUMP_COMMAND OR
    MOONLIGHT_OBJDUMP_COMMAND STREQUAL "")
    set(MOONLIGHT_OBJDUMP_COMMAND objdump)
endif ()

file(TO_CMAKE_PATH "${MOONLIGHT_RUNTIME_EXECUTABLE}" _moonlight_runtime_executable)
file(TO_CMAKE_PATH "${MOONLIGHT_RUNTIME_SEARCH_DIR}" _moonlight_runtime_search_dir)
file(TO_CMAKE_PATH "${MOONLIGHT_OBJDUMP_COMMAND}" _moonlight_objdump_command)

string(REGEX REPLACE "^\"(.*)\"$" "\\1"
    _moonlight_runtime_executable "${_moonlight_runtime_executable}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1"
    _moonlight_runtime_search_dir "${_moonlight_runtime_search_dir}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1"
    _moonlight_objdump_command "${_moonlight_objdump_command}")

if (NOT EXISTS "${_moonlight_runtime_executable}")
    message(FATAL_ERROR "Runtime executable not found: ${_moonlight_runtime_executable}")
endif ()

if (NOT EXISTS "${_moonlight_runtime_search_dir}")
    message(FATAL_ERROR "Runtime search directory not found: ${_moonlight_runtime_search_dir}")
endif ()

get_filename_component(_moonlight_runtime_output_dir
    "${_moonlight_runtime_executable}" DIRECTORY)

set(_moonlight_pending_files "${_moonlight_runtime_executable}")
set(_moonlight_scanned_files)

while (_moonlight_pending_files)
    list(POP_FRONT _moonlight_pending_files _moonlight_current_file)
    list(FIND _moonlight_scanned_files "${_moonlight_current_file}" _moonlight_seen_index)
    if (NOT _moonlight_seen_index EQUAL -1)
        continue ()
    endif ()
    list(APPEND _moonlight_scanned_files "${_moonlight_current_file}")

    execute_process(
        COMMAND "${_moonlight_objdump_command}" -p "${_moonlight_current_file}"
        RESULT_VARIABLE _moonlight_objdump_result
        OUTPUT_VARIABLE _moonlight_objdump_output
        ERROR_VARIABLE _moonlight_objdump_error)
    if (NOT _moonlight_objdump_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to inspect runtime dependencies for ${_moonlight_current_file}: ${_moonlight_objdump_error}")
    endif ()

    string(REGEX MATCHALL "DLL Name: [^\r\n]+" _moonlight_dependency_matches
        "${_moonlight_objdump_output}")

    foreach(_moonlight_dependency_match IN LISTS _moonlight_dependency_matches)
        string(REGEX REPLACE "^DLL Name: " "" _moonlight_dependency_name
            "${_moonlight_dependency_match}")
        string(STRIP "${_moonlight_dependency_name}" _moonlight_dependency_name)

        set(_moonlight_dependency_path
            "${_moonlight_runtime_search_dir}/${_moonlight_dependency_name}")
        if (EXISTS "${_moonlight_dependency_path}")
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_moonlight_dependency_path}" "${_moonlight_runtime_output_dir}"
                COMMAND_ERROR_IS_FATAL ANY)
            list(APPEND _moonlight_pending_files "${_moonlight_dependency_path}")
        endif ()
    endforeach ()
endwhile ()