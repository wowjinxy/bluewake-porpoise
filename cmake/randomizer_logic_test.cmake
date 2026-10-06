# Caller enables testing and chooses its own test guard. Windows source-only
# CI uses BLUEWAKE_WINDOWS_REGRESSION_TESTS=ON with BUILD_TESTING=OFF.
# Standard library only: no donor, guest memory, devices, assets or Python.
include_guard(GLOBAL)
get_filename_component(_bw_randomizer_test_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/BlueWakeRandomizer.cmake")
find_package(Threads REQUIRED)
if(NOT TARGET bluewake_randomizer_logic_test)
  add_executable(bluewake_randomizer_logic_test
    "${_bw_randomizer_test_repo}/tests/randomizer_logic_test.cpp")
  target_link_libraries(bluewake_randomizer_logic_test PRIVATE bluewake_randomizer Threads::Threads)
  # Keep all checks enabled in Release and preserve caller optimization/ASan.
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(bluewake_randomizer_logic_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(bluewake_randomizer_logic_test PRIVATE -UNDEBUG)
  endif()
  add_test(NAME bluewake_randomizer_logic_test COMMAND bluewake_randomizer_logic_test)
  set_tests_properties(bluewake_randomizer_logic_test PROPERTIES TIMEOUT 60 LABELS "source-only;randomizer")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/randomizer_native_inventory_test.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/randomizer_seed_test.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/randomizer_transaction_test.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/randomizer_store_test.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/randomizer_actor_owner_test.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/randomizer_rel_owner_test.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/randomizer_session_test.cmake")
