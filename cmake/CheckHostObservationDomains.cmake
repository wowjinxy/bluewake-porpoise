# This wrapper itself requires only CMake. Missing Python, changed sources,
# unknown profiles, or an unsuccessful checker all produce the ordinary path.
set(_bw_value 0)
if(BW_ENABLE STREQUAL "1" AND NOT BW_PYTHON STREQUAL "")
    execute_process(
        COMMAND "${BW_PYTHON}" -B -S -X utf8
            "${BW_ROOT}/scripts/check_host_observation_domains.py"
            --root "${BW_ROOT}"
            --certificate "${BW_ROOT}/runtime/host/src/host_observation_domains.json"
            --compiler-id "${BW_COMPILER_ID}"
            --compiler-version "${BW_COMPILER_VERSION}"
            --system "${BW_SYSTEM}"
            --system-processor "${BW_SYSTEM_PROCESSOR}"
            --compiler-target "${BW_COMPILER_TARGET}"
            --pointer-bytes "${BW_POINTER_BYTES}"
            --configuration "${BW_CONFIGURATION}"
            --developer "${BW_DEVELOPER}"
            --census "${BW_CENSUS}"
            --collector "${BW_COLLECTOR}"
            --reward "${BW_REWARD}"
        RESULT_VARIABLE _bw_result OUTPUT_VARIABLE _bw_output
        ERROR_VARIABLE _bw_error TIMEOUT 15)
    string(STRIP "${_bw_output}" _bw_output)
    if(_bw_result STREQUAL "0" AND _bw_output STREQUAL "CERTIFIED")
        set(_bw_value 1)
    endif()
endif()
set(_bw_content "/* Build-generated: do not hand-enable a stale certificate. */\n#if defined(_M_X64) || defined(__x86_64__)\n#define BLUEWAKE_MAINCODE_OBSERVATION_CERTIFIED ${_bw_value}\n#else\n#define BLUEWAKE_MAINCODE_OBSERVATION_CERTIFIED 0\n#endif\n")
if(EXISTS "${BW_HEADER}")
    file(READ "${BW_HEADER}" _bw_old)
else()
    set(_bw_old "")
endif()
if(NOT _bw_old STREQUAL _bw_content)
    file(WRITE "${BW_HEADER}" "${_bw_content}")
endif()
