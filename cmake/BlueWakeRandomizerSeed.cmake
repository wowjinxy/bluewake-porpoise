# Declarative profiles and copied-value native transactions; not linked into
# the game by this module. Storage has no guest/native/module dependency.
include_guard(GLOBAL)
get_filename_component(_bw_seed_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/BlueWakeRandomizer.cmake")
find_package(Threads REQUIRED)
if(NOT TARGET bluewake_randomizer_seed)
  add_library(bluewake_randomizer_seed STATIC
    "${_bw_seed_repo}/runtime/host/src/randomizer_seed.cpp"
    "${_bw_seed_repo}/runtime/host/src/randomizer_transaction.cpp"
    "${_bw_seed_repo}/runtime/host/src/randomizer_store.cpp")
  target_include_directories(bluewake_randomizer_seed PUBLIC "${_bw_seed_repo}/runtime/host/src")
  target_compile_features(bluewake_randomizer_seed PUBLIC cxx_std_17)
  set_target_properties(bluewake_randomizer_seed PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
  target_link_libraries(bluewake_randomizer_seed PUBLIC bluewake_randomizer Threads::Threads)
endif()
