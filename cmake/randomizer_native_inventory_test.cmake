# The caller enables testing and owns its guard. In particular, explicit Windows
# regressions must work with BUILD_TESTING=OFF. Synthetic copied values only;
# this executable contains no native reader, game module, devices or assets.
include_guard(GLOBAL)
get_filename_component(_bw_native_inventory_test_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/BlueWakeRandomizerNativeContext.cmake")
find_package(Threads REQUIRED)
if(NOT TARGET bluewake_randomizer_native_inventory_test)
  add_executable(bluewake_randomizer_native_inventory_test
    "${_bw_native_inventory_test_repo}/tests/native_inventory_context_test.cpp")
  target_link_libraries(bluewake_randomizer_native_inventory_test PRIVATE
    bluewake_randomizer_native_context Threads::Threads)
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(bluewake_randomizer_native_inventory_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(bluewake_randomizer_native_inventory_test PRIVATE -UNDEBUG)
  endif()
  add_test(NAME bluewake_randomizer_native_inventory_test
    COMMAND bluewake_randomizer_native_inventory_test)
  set_tests_properties(bluewake_randomizer_native_inventory_test PROPERTIES
    TIMEOUT 60 LABELS "source-only;randomizer;synthetic-observation")
endif()
