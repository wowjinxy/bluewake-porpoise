# Authored chassis predicate regression target; never linked into the shipping host.
include_guard(GLOBAL)
get_filename_component(_bw_precision_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if((BUILD_TESTING OR BLUEWAKE_WINDOWS_REGRESSION_TESTS) AND NOT TARGET bluewake_host_chassis_precision_test)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(_bw_precision_tests "${_bw_precision_repo}/tests/host_chassis_precision")
  set(_bw_precision_generated "${CMAKE_CURRENT_BINARY_DIR}/host-chassis-precision")
  execute_process(COMMAND "${Python3_EXECUTABLE}" -B -S
    "${_bw_precision_tests}/prepare_predicates.py" --repo-root "${_bw_precision_repo}"
    --output-dir "${_bw_precision_generated}"
    RESULT_VARIABLE _bw_precision_prepare_result ERROR_VARIABLE _bw_precision_prepare_error)
  if(NOT _bw_precision_prepare_result EQUAL 0)
    message(FATAL_ERROR "Host chassis precision chain guard: ${_bw_precision_prepare_error}")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_bw_precision_repo}/runtime/host/src/main.c"
    "${_bw_precision_repo}/runtime/host/src/edge_intercepts.c"
    "${_bw_precision_repo}/runtime/host/src/edge_intercept_table.h"
    "${_bw_precision_tests}/predicates.inc" "${_bw_precision_tests}/prepare_predicates.py")
  add_executable(bluewake_host_chassis_precision_test "${_bw_precision_tests}/oracle.c")
  target_include_directories(bluewake_host_chassis_precision_test PRIVATE
    "${_bw_precision_repo}/runtime/host/src" "${_bw_precision_generated}"
    "${_bw_precision_repo}/ref/recompcore/GXRuntime/include"
    "${_bw_precision_repo}/ref/recompcore/Source/Core/Core/PowerPC/StaticRecomp")
  target_compile_features(bluewake_host_chassis_precision_test PRIVATE c_std_17)
  option(BLUEWAKE_HOST_PRECISION_ASAN "Instrument the source-only observer fixture" OFF)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU" AND NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(bluewake_host_chassis_precision_test PRIVATE -UNDEBUG -ffunction-sections -fdata-sections)
    if(BLUEWAKE_HOST_PRECISION_ASAN)
      target_compile_options(bluewake_host_chassis_precision_test PRIVATE -O2 -fsanitize=address -fno-omit-frame-pointer)
      target_link_options(bluewake_host_chassis_precision_test PRIVATE -fsanitize=address)
    else()
      target_compile_options(bluewake_host_chassis_precision_test PRIVATE -O3)
    endif()
    if(WIN32)
      if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(bluewake_host_chassis_precision_test PRIVATE -fms-compatibility-version=19.44 -fms-runtime-lib=dll)
        target_link_options(bluewake_host_chassis_precision_test PRIVATE -fms-runtime-lib=dll)
        if(BLUEWAKE_HOST_PRECISION_ASAN)
          # CMake's Windows Clang link uses -nostdlib; request the DLL ASan
          # runtime explicitly to match the dynamic CRT used by these TUs.
          target_link_options(bluewake_host_chassis_precision_test PRIVATE -shared-libasan)
        endif()
      endif()
      target_link_options(bluewake_host_chassis_precision_test PRIVATE -Wl,/OPT:REF)
    elseif(APPLE)
      target_link_options(bluewake_host_chassis_precision_test PRIVATE -Wl,-dead_strip)
      target_link_libraries(bluewake_host_chassis_precision_test PRIVATE m)
    else()
      target_link_options(bluewake_host_chassis_precision_test PRIVATE -Wl,--gc-sections)
      target_link_libraries(bluewake_host_chassis_precision_test PRIVATE m)
    endif()
  else()
    message(FATAL_ERROR "Host chassis precision regression requires a GNU-style Clang/GCC C17 driver")
  endif()
  add_test(NAME bluewake_host_chassis_precision_test COMMAND bluewake_host_chassis_precision_test)
  set_tests_properties(bluewake_host_chassis_precision_test PROPERTIES TIMEOUT 120 LABELS "source-only;chassis;synthetic")
endif()
