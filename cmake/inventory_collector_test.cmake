# Caller owns enable_testing and its existing test guard. All registered
# default tests use synthetic data; no game or UI/device runtime is linked.
include_guard(GLOBAL)
get_filename_component(_bw_collector_test_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/BlueWakeRandomizerNativeContext.cmake")
find_package(Threads REQUIRED)
set(BLUEWAKE_INVENTORY_COLLECTOR_TEST_METADATA "" CACHE FILEPATH
  "Optional private canonical descriptor for the SYNTHETIC mocked-lease bridge")
set(BLUEWAKE_INVENTORY_COLLECTOR_TEST_WINDOWS_POLICY "" CACHE FILEPATH
  "Optional external policy for the authored tiny Windows fixture DLL")
set(BLUEWAKE_INVENTORY_COLLECTOR_TEST_WINDOWS_POLICY_SHA256 "" CACHE STRING
  "Explicit test-owner approval of the tiny fixture DLL policy, never a game policy")
if(NOT DEFINED GXRUNTIME_DIR)
  set(GXRUNTIME_DIR "${_bw_collector_test_repo}/ref/recompcore/GXRuntime")
endif()
get_filename_component(_bw_collector_donor "${GXRUNTIME_DIR}/.." ABSOLUTE)

function(_bluewake_collector_fixture_common target_name)
  target_include_directories("${target_name}" PRIVATE
    "${_bw_collector_test_repo}/runtime/host/src"
    "${GXRUNTIME_DIR}/include"
    "${_bw_collector_donor}/Source/Core/Core/PowerPC/StaticRecomp")
  target_compile_definitions("${target_name}" PRIVATE DOLRECOMP_CPU_HEADER="core/cpu.h")
  target_compile_features("${target_name}" PRIVATE cxx_std_17)
  target_link_libraries("${target_name}" PRIVATE Threads::Threads)
  if(WIN32)
    target_include_directories("${target_name}" PRIVATE ${BLUEWAKE_COMPAT_INCLUDES})
    target_compile_options("${target_name}" PRIVATE ${BLUEWAKE_COMPAT_FORCE_INCLUDE})
    target_compile_definitions("${target_name}" PRIVATE
      _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
    if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      # Retain ASan ODR checks; readonly callback/literal folding is disabled.
      target_link_options("${target_name}" PRIVATE "LINKER:/OPT:NOICF")
    endif()
  endif()
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options("${target_name}" PRIVATE /UNDEBUG)
  else()
    target_compile_options("${target_name}" PRIVATE -UNDEBUG)
  endif()
endfunction()

function(_bluewake_collector_bridge target_name source_name use_mock)
  add_executable("${target_name}"
    "${_bw_collector_test_repo}/tests/${source_name}"
    "${_bw_collector_test_repo}/runtime/host/src/inventory_collector.cpp"
    "${_bw_collector_test_repo}/runtime/host/src/inventory_collector_host.cpp"
    "${_bw_collector_test_repo}/runtime/host/src/game_events.c"
    "${GXRUNTIME_DIR}/src/core/cpu.c"
    "${GXRUNTIME_DIR}/src/core/cpu_exception.c"
    "${GXRUNTIME_DIR}/src/core/cpu_interpreter.c"
    "${GXRUNTIME_DIR}/src/core/cpu_interpreter_table.c"
    "${GXRUNTIME_DIR}/src/core/cpu_interpreter_float.c"
    "${GXRUNTIME_DIR}/src/core/cpu_interpreter_integer.c")
  if(use_mock)
    # Mock symbols are confined to this explicitly synthetic target.
    target_sources("${target_name}" PRIVATE
      "${_bw_collector_test_repo}/tests/inventory_collector_lease_mock.cpp")
  else()
    target_sources("${target_name}" PRIVATE
      "${_bw_collector_test_repo}/runtime/host/src/loaded_code_admission.cpp"
      "${_bw_collector_test_repo}/runtime/host/src/loaded_code_image.cpp")
    if(WIN32)
      target_link_libraries("${target_name}" PRIVATE bcrypt uuid)
    endif()
  endif()
  _bluewake_collector_fixture_common("${target_name}")
  target_compile_definitions("${target_name}" PRIVATE BW_NATIVE_INVENTORY_COLLECTOR=1)
  target_compile_features("${target_name}" PRIVATE c_std_11)
  target_link_libraries("${target_name}" PRIVATE bluewake_randomizer_native_context)
  if(UNIX)
    target_link_libraries("${target_name}" PRIVATE m)
  endif()
endfunction()

if(NOT TARGET bluewake_inventory_collector_test)
  _bluewake_collector_bridge(bluewake_inventory_collector_test inventory_collector_test.cpp FALSE)
  add_test(NAME bluewake_inventory_collector_test COMMAND bluewake_inventory_collector_test)
  set_tests_properties(bluewake_inventory_collector_test PROPERTIES
    TIMEOUT 60 LABELS "source-only;randomizer;synthetic-observation")
endif()
if(NOT "${BLUEWAKE_INVENTORY_COLLECTOR_TEST_METADATA}" STREQUAL "")
  if(NOT EXISTS "${BLUEWAKE_INVENTORY_COLLECTOR_TEST_METADATA}")
    message(FATAL_ERROR "Explicit synthetic-bridge metadata does not exist")
  endif()
  if(NOT TARGET bluewake_inventory_collector_owned_metadata_test)
    _bluewake_collector_bridge(bluewake_inventory_collector_owned_metadata_test
      inventory_collector_owned_metadata_test.cpp TRUE)
    add_test(NAME bluewake_inventory_collector_owned_metadata_test
      COMMAND bluewake_inventory_collector_owned_metadata_test
        "${BLUEWAKE_INVENTORY_COLLECTOR_TEST_METADATA}")
    set_tests_properties(bluewake_inventory_collector_owned_metadata_test PROPERTIES
      TIMEOUT 60 LABELS "source-only;randomizer;synthetic-mocked-lease")
  endif()
endif()
if(NOT TARGET bluewake_loaded_code_image_test)
  add_executable(bluewake_loaded_code_image_test
    "${_bw_collector_test_repo}/tests/loaded_code_image_test.cpp"
    "${_bw_collector_test_repo}/runtime/host/src/loaded_code_image.cpp")
  _bluewake_collector_fixture_common(bluewake_loaded_code_image_test)
  add_test(NAME bluewake_loaded_code_image_test COMMAND bluewake_loaded_code_image_test)
  set_tests_properties(bluewake_loaded_code_image_test PROPERTIES
    TIMEOUT 60 LABELS "source-only;randomizer;synthetic-pe-image")
endif()
if(WIN32 AND NOT TARGET bluewake_loaded_code_windows_test)
  add_library(bluewake_loaded_code_fixture MODULE
    "${_bw_collector_test_repo}/tests/loaded_code_fixture_dll.cpp")
  _bluewake_collector_fixture_common(bluewake_loaded_code_fixture)
  set_target_properties(bluewake_loaded_code_fixture PROPERTIES PREFIX "")
  add_executable(bluewake_loaded_code_windows_test
    "${_bw_collector_test_repo}/tests/loaded_code_windows_test.cpp"
    "${_bw_collector_test_repo}/runtime/host/src/loaded_code_admission.cpp"
    "${_bw_collector_test_repo}/runtime/host/src/loaded_code_image.cpp")
  _bluewake_collector_fixture_common(bluewake_loaded_code_windows_test)
  target_link_libraries(bluewake_loaded_code_windows_test PRIVATE bcrypt uuid)
  add_dependencies(bluewake_loaded_code_windows_test bluewake_loaded_code_fixture)
  # No implicit policy generation or default DLL loading. Only a separately
  # reviewed external policy for this authored fixture can register this test.
  if(NOT "${BLUEWAKE_INVENTORY_COLLECTOR_TEST_WINDOWS_POLICY}" STREQUAL "" OR
     NOT "${BLUEWAKE_INVENTORY_COLLECTOR_TEST_WINDOWS_POLICY_SHA256}" STREQUAL "")
    string(LENGTH "${BLUEWAKE_INVENTORY_COLLECTOR_TEST_WINDOWS_POLICY_SHA256}" _bw_fixture_policy_length)
    if(NOT EXISTS "${BLUEWAKE_INVENTORY_COLLECTOR_TEST_WINDOWS_POLICY}" OR
       NOT _bw_fixture_policy_length EQUAL 64 OR
       NOT BLUEWAKE_INVENTORY_COLLECTOR_TEST_WINDOWS_POLICY_SHA256 MATCHES "^[0-9a-f]+$")
      message(FATAL_ERROR "Windows lease fixture requires an existing external policy and its explicit SHA256 approval")
    endif()
    add_test(NAME bluewake_loaded_code_windows_test
      COMMAND bluewake_loaded_code_windows_test "$<TARGET_FILE:bluewake_loaded_code_fixture>"
        "${BLUEWAKE_INVENTORY_COLLECTOR_TEST_WINDOWS_POLICY}"
        "${BLUEWAKE_INVENTORY_COLLECTOR_TEST_WINDOWS_POLICY_SHA256}")
    set_tests_properties(bluewake_loaded_code_windows_test PROPERTIES
      TIMEOUT 60 LABELS "source-only;randomizer;synthetic-windows-dll")
  endif()
endif()
