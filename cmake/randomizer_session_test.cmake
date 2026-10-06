# Source-only session/lease ownership, synthetic records and actual opaque store.
include_guard(GLOBAL)
get_filename_component(_bw_session_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/BlueWakeRandomizerSeed.cmake")
if(NOT TARGET bluewake_randomizer_session_test)
  add_executable(bluewake_randomizer_session_test
    "${_bw_session_repo}/tests/randomizer_session_test.cpp"
    "${_bw_session_repo}/runtime/host/src/randomizer_session.cpp")
  target_link_libraries(bluewake_randomizer_session_test PRIVATE bluewake_randomizer_seed)
  set_target_properties(bluewake_randomizer_session_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(bluewake_randomizer_session_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(bluewake_randomizer_session_test PRIVATE -UNDEBUG)
  endif()
  add_test(NAME bluewake_randomizer_session_test COMMAND bluewake_randomizer_session_test "${CMAKE_CURRENT_BINARY_DIR}/randomizer-sessions")
  set_tests_properties(bluewake_randomizer_session_test PROPERTIES TIMEOUT 60 LABELS "source-only;randomizer;storage;synthetic-observation")
endif()
