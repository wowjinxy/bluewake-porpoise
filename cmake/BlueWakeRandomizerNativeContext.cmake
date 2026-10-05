# Value-only projection over copied observations and a closed capability set.
# Passive library only: no host reader, runtime integration, UI or seed API.
include_guard(GLOBAL)
get_filename_component(_bw_native_context_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/BlueWakeRandomizer.cmake")
if(NOT TARGET bluewake_randomizer_native_context)
  add_library(bluewake_randomizer_native_context STATIC
    "${_bw_native_context_repo}/runtime/host/src/native_inventory_context.cpp")
  target_include_directories(bluewake_randomizer_native_context PUBLIC
    "${_bw_native_context_repo}/runtime/host/src")
  target_compile_features(bluewake_randomizer_native_context PUBLIC cxx_std_17)
  target_link_libraries(bluewake_randomizer_native_context PUBLIC bluewake_randomizer)
endif()
