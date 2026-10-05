# Authored same-chunk regression. No disc/module assets or device initialization.
if(BUILD_TESTING AND NOT TARGET bluewake_healing_return_test)
  include("${CMAKE_CURRENT_LIST_DIR}/BlueWakeHealthRulesPrototype.cmake")
  set(_healing_root "${CMAKE_CURRENT_LIST_DIR}/..")
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${_healing_root}/tests/healing_return_prepare.py" "${CMAKE_CURRENT_BINARY_DIR}"
    RESULT_VARIABLE _healing_prepare ERROR_VARIABLE _healing_error)
  if(NOT _healing_prepare EQUAL 0)
    message(FATAL_ERROR "Healing topology preparation failed: ${_healing_error}")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_healing_root}/scripts/windows/direct_calls.py" "${_healing_root}/tests/healing_return_chunk.c.in")
  add_executable(bluewake_healing_return_test "${_healing_root}/tests/healing_return_test.c"
    "${_healing_root}/cmake/composite/direct_calls.c")
  target_include_directories(bluewake_healing_return_test PRIVATE "${_healing_root}/runtime/host/src"
    "${_healing_root}/ref/recompcore/GXRuntime/include" "${_healing_root}/cmake/composite"
    "${_healing_root}/tests" "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(bluewake_healing_return_test PRIVATE BLUEWAKE_DIRECT_CALLS=1 BLUEWAKE_EDGE_FILTER=1
    BLUEWAKE_HEALING_RETURN_CERTIFIED=1 BW_EDGE_WATCH_INCLUDE="direct_call_watch.inc")
  target_compile_features(bluewake_healing_return_test PRIVATE c_std_11)
  target_link_libraries(bluewake_healing_return_test PRIVATE bluewake_health_rules_prototype)
  if(TARGET gxruntime)
    target_link_libraries(bluewake_healing_return_test PRIVATE gxruntime)
    set_target_properties(bluewake_healing_return_test PROPERTIES LINKER_LANGUAGE CXX)
  else()
    target_sources(bluewake_healing_return_test PRIVATE "${_healing_root}/ref/recompcore/GXRuntime/src/guest_memory.c"
      "${_healing_root}/ref/recompcore/GXRuntime/src/guest_memory_dirty.c" "${_healing_root}/ref/recompcore/GXRuntime/src/core/cpu.c"
      "${_healing_root}/ref/recompcore/GXRuntime/src/core/cpu_exception.c")
    target_compile_options(bluewake_healing_return_test PRIVATE -ffunction-sections -fdata-sections)
    if(WIN32)
      target_link_options(bluewake_healing_return_test PRIVATE -Wl,/OPT:REF)
    else()
      target_link_options(bluewake_healing_return_test PRIVATE -Wl,--gc-sections)
      target_link_libraries(bluewake_healing_return_test PRIVATE m)
    endif()
  endif()
  add_test(NAME bluewake_healing_return_test COMMAND bluewake_healing_return_test)
  set_tests_properties(bluewake_healing_return_test PROPERTIES TIMEOUT 30)
endif()
