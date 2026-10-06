# Caller owns enable_testing and its existing regression guard. Authored
# synthetic host services are confined to this executable, never game authority.
# Link the real core alias resolver; do not copy a prefix of cpu.c into tests.
include_guard(GLOBAL)
if(NOT TARGET GXRuntime::runtime)
  message(FATAL_ERROR "Reward host regressions require the existing GXRuntime core target")
endif()
if(NOT DEFINED GXRUNTIME_DIR)
  message(FATAL_ERROR "Reward host regressions require the caller's GXRuntime source root")
endif()
get_filename_component(_bw_reward_test_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(_bw_reward_test_donor "${GXRUNTIME_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/BlueWakeRandomizerSeed.cmake")
find_package(Threads REQUIRED)
if(NOT TARGET bluewake_randomizer_reward_host_test)
  add_executable(bluewake_randomizer_reward_host_test
    "${_bw_reward_test_repo}/tests/randomizer_reward_host_test.cpp"
    "${_bw_reward_test_repo}/tests/randomizer_reward_host_support.cpp"
    "${_bw_reward_test_repo}/runtime/host/src/randomizer_reward_host.cpp"
    "${_bw_reward_test_repo}/runtime/host/src/randomizer_session.cpp"
    "${_bw_reward_test_repo}/runtime/host/src/randomizer_card_payload.cpp"
    "${_bw_reward_test_repo}/runtime/host/src/randomizer_actor_owner.c"
    "${_bw_reward_test_repo}/runtime/host/src/randomizer_rel_owner.c")
  target_include_directories(bluewake_randomizer_reward_host_test PRIVATE
    "${_bw_reward_test_repo}/runtime/host/src"
    "${GXRUNTIME_DIR}/include"
    "${_bw_reward_test_donor}/Source/Core/Core/PowerPC/StaticRecomp")
  target_compile_definitions(bluewake_randomizer_reward_host_test PRIVATE
    DOLRECOMP_CPU_HEADER="core/cpu.h")
  target_compile_features(bluewake_randomizer_reward_host_test PRIVATE c_std_17 cxx_std_17)
  set_target_properties(bluewake_randomizer_reward_host_test PROPERTIES
    C_STANDARD 17 C_STANDARD_REQUIRED YES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
  target_link_libraries(bluewake_randomizer_reward_host_test PRIVATE
    GXRuntime::runtime bluewake_randomizer_seed Threads::Threads)
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(bluewake_randomizer_reward_host_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(bluewake_randomizer_reward_host_test PRIVATE -UNDEBUG)
  endif()
  if(WIN32 AND (MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    # Preserve ASan ODR checking; do not fold readonly literal/callback globals.
    target_link_options(bluewake_randomizer_reward_host_test PRIVATE "LINKER:/OPT:NOICF")
  endif()
  add_test(NAME bluewake_randomizer_reward_host_test
    COMMAND bluewake_randomizer_reward_host_test
      "${CMAKE_CURRENT_BINARY_DIR}/randomizer-reward-host-synthetic")
  set_tests_properties(bluewake_randomizer_reward_host_test PROPERTIES
    TIMEOUT 120 LABELS "source-only;randomizer;authored-synthetic;no-native-module;no-device")
endif()
