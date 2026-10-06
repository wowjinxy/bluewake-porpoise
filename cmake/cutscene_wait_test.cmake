# Authored synthetic core fixture only: issuer/resolver/owner callbacks are stand-ins.
# No host, native function, game assets, window or device is started.
include_guard(GLOBAL)
get_filename_component(_bw_tc_wait_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT TARGET bluewake_cutscene_wait_test)
  add_executable(bluewake_cutscene_wait_test
    "${_bw_tc_wait_repo}/runtime/host/src/cutscene_wait.c"
    "${_bw_tc_wait_repo}/tests/cutscene_wait_test.c")
  target_include_directories(bluewake_cutscene_wait_test PRIVATE
    "${_bw_tc_wait_repo}/runtime/host/src"
    "${_bw_tc_wait_repo}/ref/recompcore/GXRuntime/include")
  target_compile_features(bluewake_cutscene_wait_test PRIVATE c_std_17)
  if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(bluewake_cutscene_wait_test PRIVATE /O2 /UNDEBUG)
  else()
    target_compile_options(bluewake_cutscene_wait_test PRIVATE -O3 -UNDEBUG)
  endif()
  add_test(NAME bluewake_cutscene_wait_test COMMAND bluewake_cutscene_wait_test)
  set_tests_properties(bluewake_cutscene_wait_test PROPERTIES
    TIMEOUT 120 LABELS "source-only;cutscene;synthetic")
endif()
