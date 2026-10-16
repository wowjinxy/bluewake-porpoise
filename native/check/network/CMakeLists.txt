# Standalone copied-value tests: no game, SDK, Aurora, asset generation or downloads.
cmake_minimum_required(VERSION 3.25)
project(cos_network_checks LANGUAGES CXX)

# Match the native root's default; network.cmake keeps its transport targets at C++17.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(COS_NATIVE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
include("${COS_NATIVE_ROOT}/cmake/network.cmake")

enable_testing()
set(_fixture_root "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
message(STATUS "Standalone network synthetic fixtures: ${_fixture_root}")
set(_network_tests
    cos_network_schema_tests
    cos_network_transport_tests
    cos_network_presence_tests
    cos_network_store_tests
    cos_network_profile_tests
    cos_progression_logic_tests
    cos_network_game_policy_tests
    cos_network_client_store_tests)
foreach(_test IN LISTS _network_tests)
    file(MAKE_DIRECTORY "${_fixture_root}/${_test}")
    if(_test STREQUAL "cos_network_profile_tests")
        # The profile test refuses an existing fixture. Retain each invocation's
        # synthetic files and use a fresh name even when CTest is run again.
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/run-profile.cmake" CONTENT [=[
string(RANDOM LENGTH 24 ALPHABET 0123456789abcdef _fixture)
execute_process(
    COMMAND "$<TARGET_FILE:cos_network_profile_tests>"
        "$ENV{TMP}/profile-fixture-${_fixture}"
    RESULT_VARIABLE _result)
if(NOT _result STREQUAL "0")
    message(FATAL_ERROR "Standalone network profile test failed: ${_result}")
endif()
]=])
        add_test(NAME "${_test}" COMMAND "${CMAKE_COMMAND}"
            -P "${CMAKE_CURRENT_BINARY_DIR}/run-profile.cmake")
    else()
        add_test(NAME "${_test}" COMMAND "${_test}")
    endif()
    set_tests_properties("${_test}" PROPERTIES ENVIRONMENT
        "TEMP=${_fixture_root}/${_test};TMP=${_fixture_root}/${_test};TMPDIR=${_fixture_root}/${_test}")
endforeach()
set_tests_properties(${_network_tests} PROPERTIES TIMEOUT 300 LABELS "network;standalone")
