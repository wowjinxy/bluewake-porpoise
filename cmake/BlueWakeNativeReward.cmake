# Experimental single-location native reward adapter. Default OFF contributes
# no host sources, defines, libraries, subscriptions or observation hooks.
include_guard(GLOBAL)
option(BLUEWAKE_NATIVE_REWARD_SESSION
  "Build experimental headless LinkUG seed sessions" OFF)

function(bluewake_enable_native_reward target_name)
  if(NOT BLUEWAKE_NATIVE_REWARD_SESSION)
    return()
  endif()
  if(NOT WIN32 OR NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "Native seed sessions currently require the Windows admitted-code owner and Clang/GNU cleanup")
  endif()
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR "Native seed sessions require an existing host target")
  endif()
  # Share the existing admitted-code scope. Never create a second module lease
  # or quietly enable a collector the caller did not request.
  get_target_property(_bw_reward_admission "${target_name}" BLUEWAKE_COLLECTOR_WIRED)
  if(NOT BLUEWAKE_NATIVE_INVENTORY_COLLECTOR OR NOT _bw_reward_admission)
    message(FATAL_ERROR "Native seed sessions require BLUEWAKE_NATIVE_INVENTORY_COLLECTOR=ON on this host")
  endif()
  get_target_property(_bw_reward_wired "${target_name}" BLUEWAKE_NATIVE_REWARD_WIRED)
  if(_bw_reward_wired)
    return()
  endif()
  get_filename_component(_bw_reward_repo "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  include("${_bw_reward_repo}/cmake/BlueWakeRandomizerSeed.cmake")
  if(NOT TARGET bluewake_native_reward)
    add_library(bluewake_native_reward STATIC
      "${_bw_reward_repo}/runtime/host/src/randomizer_reward_host.cpp"
      "${_bw_reward_repo}/runtime/host/src/randomizer_session.cpp"
      "${_bw_reward_repo}/runtime/host/src/randomizer_card_payload.cpp"
      "${_bw_reward_repo}/runtime/host/src/randomizer_actor_owner.c"
      "${_bw_reward_repo}/runtime/host/src/randomizer_rel_owner.c")
    target_include_directories(bluewake_native_reward PUBLIC
      "${_bw_reward_repo}/runtime/host/src"
      "${GXRUNTIME_DIR}/include"
      "${GXRUNTIME_DIR}/../Source/Core/Core/PowerPC/StaticRecomp")
    target_compile_definitions(bluewake_native_reward PRIVATE
      DOLRECOMP_CPU_HEADER="core/cpu.h")
    target_compile_features(bluewake_native_reward PRIVATE cxx_std_17)
    set_target_properties(bluewake_native_reward PROPERTIES
      C_STANDARD 17 C_STANDARD_REQUIRED YES
      CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
    target_link_libraries(bluewake_native_reward PUBLIC bluewake_randomizer_seed)
  endif()
  target_compile_definitions("${target_name}" PRIVATE BW_NATIVE_REWARD_SESSION=1)
  target_link_libraries("${target_name}" PRIVATE bluewake_native_reward)
  set_property(TARGET "${target_name}" PROPERTY BLUEWAKE_NATIVE_REWARD_WIRED TRUE)
endfunction()
