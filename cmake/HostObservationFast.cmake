# Optional sufficient-negative optimization. Source or profile drift disables
# it; it must never make an otherwise supported host configuration unusable.
include_guard(GLOBAL)
option(BLUEWAKE_ENABLE_MAINCODE_OBSERVATION_FAST
       "Enable the certified main-code observation shortcut" ON)

function(bluewake_enable_host_observation_fast target_name developer_tracing edge_census)
    get_filename_component(_bw_repo "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(_bw_certificate_dir "${CMAKE_CURRENT_BINARY_DIR}/host_observation_certificate")
    set(_bw_header "${_bw_certificate_dir}/host_observation_certificate.h")
    file(MAKE_DIRECTORY "${_bw_certificate_dir}")
    if(NOT EXISTS "${_bw_header}")
        file(WRITE "${_bw_header}" "#define BLUEWAKE_MAINCODE_OBSERVATION_CERTIFIED 0\n")
    endif()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    # An always-run prerequisite catches edits after configure, including a
    # removed/missing predicate file. The header changes only if its value does.
    add_custom_target(bluewake_host_observation_certificate
        COMMAND "${CMAKE_COMMAND}"
            "-DBW_ROOT=${_bw_repo}"
            "-DBW_HEADER=${_bw_header}"
            "-DBW_PYTHON=${Python3_EXECUTABLE}"
            "-DBW_ENABLE=$<BOOL:${BLUEWAKE_ENABLE_MAINCODE_OBSERVATION_FAST}>"
            "-DBW_COMPILER_ID=${CMAKE_C_COMPILER_ID}"
            "-DBW_COMPILER_VERSION=${CMAKE_C_COMPILER_VERSION}"
            "-DBW_SYSTEM=${CMAKE_SYSTEM_NAME}"
            "-DBW_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}"
            "-DBW_COMPILER_TARGET=${CMAKE_C_COMPILER_TARGET}"
            "-DBW_POINTER_BYTES=${CMAKE_SIZEOF_VOID_P}"
            "-DBW_CONFIGURATION=$<CONFIG>"
            "-DBW_DEVELOPER=$<BOOL:${developer_tracing}>"
            "-DBW_CENSUS=$<BOOL:${edge_census}>"
            "-DBW_COLLECTOR=$<BOOL:${BLUEWAKE_NATIVE_INVENTORY_COLLECTOR}>"
            "-DBW_REWARD=$<BOOL:${BLUEWAKE_NATIVE_REWARD_SESSION}>"
            -P "${_bw_repo}/cmake/CheckHostObservationDomains.cmake"
        BYPRODUCTS "${_bw_header}"
        VERBATIM)
    add_dependencies("${target_name}" bluewake_host_observation_certificate)
    set_property(SOURCE "${_bw_repo}/runtime/host/src/main.c" APPEND PROPERTY
        OBJECT_DEPENDS "${_bw_header}")
    target_include_directories("${target_name}" PRIVATE "${_bw_certificate_dir}")
    target_compile_definitions("${target_name}" PRIVATE
        BLUEWAKE_MAINCODE_OBSERVATION_CERTIFICATE_AVAILABLE=1)
endfunction()
