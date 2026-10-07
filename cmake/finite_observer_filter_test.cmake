# Explicit source-only regression target; never linked into the shipping host.
include_guard(GLOBAL)
get_filename_component(_bw_observer_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if((BUILD_TESTING OR BLUEWAKE_WINDOWS_REGRESSION_TESTS) AND NOT TARGET bluewake_finite_observer_filter_test)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(_bw_observer_tests "${_bw_observer_repo}/tests/finite_observer_filter")
  set(_bw_observer_generated "${CMAKE_CURRENT_BINARY_DIR}/finite-observer-filter")
  execute_process(COMMAND "${Python3_EXECUTABLE}" -B -S
    "${_bw_observer_tests}/prepare_chains.py" --repo-root "${_bw_observer_repo}"
    --output-dir "${_bw_observer_generated}"
    RESULT_VARIABLE _bw_observer_prepare_result ERROR_VARIABLE _bw_observer_prepare_error)
  if(NOT _bw_observer_prepare_result EQUAL 0)
    message(FATAL_ERROR "Finite observer chain guard: ${_bw_observer_prepare_error}")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_bw_observer_repo}/runtime/host/src/main.c" "${_bw_observer_tests}/chains.inc")
  add_executable(bluewake_finite_observer_filter_test
    "${_bw_observer_tests}/hud_host_wrapper.c"
    "${_bw_observer_tests}/health_host_wrapper.c"
    "${_bw_observer_tests}/quick_items_wrapper.c"
    "${_bw_observer_tests}/dialogue_speed_wrapper.c"
    "${_bw_observer_tests}/enhancement_hooks_wrapper.c"
    "${_bw_observer_tests}/autosave_wrapper.c"
    "${_bw_observer_tests}/hud_customization_wrapper.c"
    "${_bw_observer_tests}/health_rules_wrapper.c"
    "${_bw_observer_tests}/oracle.c")
  target_include_directories(bluewake_finite_observer_filter_test PRIVATE
    "${_bw_observer_repo}/runtime/host/src" "${_bw_observer_generated}"
    "${_bw_observer_repo}/ref/recompcore/GXRuntime/include"
    "${_bw_observer_repo}/ref/recompcore/Source/Core/Core/PowerPC/StaticRecomp")
  target_compile_features(bluewake_finite_observer_filter_test PRIVATE c_std_17)
  option(BLUEWAKE_FINITE_OBSERVER_ASAN "Instrument the source-only observer fixture" OFF)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU" AND NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(bluewake_finite_observer_filter_test PRIVATE -UNDEBUG -ffunction-sections -fdata-sections)
    if(BLUEWAKE_FINITE_OBSERVER_ASAN)
      target_compile_options(bluewake_finite_observer_filter_test PRIVATE -O2 -fsanitize=address -fno-omit-frame-pointer)
      target_link_options(bluewake_finite_observer_filter_test PRIVATE -fsanitize=address)
    else()
      target_compile_options(bluewake_finite_observer_filter_test PRIVATE -O3)
    endif()
    if(WIN32)
      if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(bluewake_finite_observer_filter_test PRIVATE -fms-compatibility-version=19.44 -fms-runtime-lib=dll)
        target_link_options(bluewake_finite_observer_filter_test PRIVATE -fms-runtime-lib=dll)
        if(BLUEWAKE_FINITE_OBSERVER_ASAN)
          # CMake's Windows Clang link uses -nostdlib; request the DLL ASan
          # runtime explicitly to match the dynamic CRT used by these TUs.
          target_link_options(bluewake_finite_observer_filter_test PRIVATE -shared-libasan)
        endif()
      endif()
      target_link_options(bluewake_finite_observer_filter_test PRIVATE -Wl,/OPT:REF)
    elseif(APPLE)
      target_link_options(bluewake_finite_observer_filter_test PRIVATE -Wl,-dead_strip)
      target_link_libraries(bluewake_finite_observer_filter_test PRIVATE m)
    else()
      target_link_options(bluewake_finite_observer_filter_test PRIVATE -Wl,--gc-sections)
      target_link_libraries(bluewake_finite_observer_filter_test PRIVATE m)
    endif()
  else()
    message(FATAL_ERROR "Finite observer regression requires a GNU-style Clang/GCC C17 driver")
  endif()
  add_test(NAME bluewake_finite_observer_filter_test COMMAND bluewake_finite_observer_filter_test)
  set_tests_properties(bluewake_finite_observer_filter_test PROPERTIES TIMEOUT 120 LABELS "source-only;observer;synthetic")
endif()
