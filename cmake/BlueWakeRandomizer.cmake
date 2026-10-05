# Passive public source catalog/logic. No host, native-context, save or seed API.
# Kept separate from every shipping executable until independently integrated.
include_guard(GLOBAL)
get_filename_component(_bw_randomizer_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT TARGET bluewake_randomizer)
  add_library(bluewake_randomizer STATIC
    "${_bw_randomizer_repo}/runtime/host/src/randomizer_catalog.cpp"
    "${_bw_randomizer_repo}/runtime/host/src/randomizer_logic.cpp")
  target_include_directories(bluewake_randomizer PUBLIC "${_bw_randomizer_repo}/runtime/host/src")
  target_compile_features(bluewake_randomizer PUBLIC cxx_std_17)
endif()
