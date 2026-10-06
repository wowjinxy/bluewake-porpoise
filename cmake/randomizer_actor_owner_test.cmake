# Caller owns enable_testing and guards, including explicit Windows tests with
# BUILD_TESTING=OFF. Synthetic non-faulting resolver; no guest runtime or device.
include_guard(GLOBAL)
get_filename_component(_bw_actor_owner_test_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT TARGET bluewake_randomizer_actor_owner_test)
  add_executable(bluewake_randomizer_actor_owner_test
    "${_bw_actor_owner_test_repo}/tests/randomizer_actor_owner_test.c"
    "${_bw_actor_owner_test_repo}/runtime/host/src/randomizer_actor_owner.c")
  target_include_directories(bluewake_randomizer_actor_owner_test PRIVATE
    "${_bw_actor_owner_test_repo}/runtime/host/src")
  target_compile_features(bluewake_randomizer_actor_owner_test PRIVATE c_std_11)
  if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(bluewake_randomizer_actor_owner_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(bluewake_randomizer_actor_owner_test PRIVATE -UNDEBUG)
  endif()
  add_test(NAME bluewake_randomizer_actor_owner_test COMMAND bluewake_randomizer_actor_owner_test)
  set_tests_properties(bluewake_randomizer_actor_owner_test PROPERTIES
    TIMEOUT 60 LABELS "source-only;randomizer;synthetic-observation")
endif()
