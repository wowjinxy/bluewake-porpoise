# Standalone/system-socket BlueWake Wind Waker networking; no Steam dependency.
if(NOT TARGET bluewake_network)
  set(_bw_network_root "${CMAKE_CURRENT_LIST_DIR}/..")
  find_package(Threads REQUIRED)
  add_library(bluewake_network STATIC
    "${_bw_network_root}/runtime/host/src/network_session.cpp"
    "${_bw_network_root}/runtime/host/src/network_store.cpp"
    "${_bw_network_root}/runtime/host/src/network_preferences.cpp"
    "${_bw_network_root}/runtime/host/src/network_game.cpp"
    "${_bw_network_root}/runtime/host/src/progression_sync.c")
  target_compile_features(bluewake_network PUBLIC cxx_std_17)
  target_include_directories(bluewake_network PUBLIC
    "${_bw_network_root}/runtime/host/src"
    "${_bw_network_root}/ref/recompcore/GXRuntime/include")
  target_link_libraries(bluewake_network PUBLIC Threads::Threads)
  if(WIN32)
    target_link_libraries(bluewake_network PUBLIC ws2_32)
    target_compile_definitions(bluewake_network PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
  endif()
endif()
