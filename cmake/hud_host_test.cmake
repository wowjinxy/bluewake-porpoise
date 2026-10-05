# Common source-only regressions after their relevant existing targets exist.
# No prepared game bytes, captured RAM or private generated includes are used.
get_filename_component(_bw_hud_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(BUILD_TESTING AND NOT TARGET bluewake_hud_core_test)
  add_library(bluewake_hud_fixture_core STATIC "${_bw_hud_repo}/runtime/host/src/hud_customization.c")
  target_include_directories(bluewake_hud_fixture_core PUBLIC "${_bw_hud_repo}/runtime/host/src")
  target_compile_features(bluewake_hud_fixture_core PUBLIC c_std_11)
  if(UNIX)
    target_link_libraries(bluewake_hud_fixture_core PUBLIC m)
  endif()
  add_executable(bluewake_hud_core_test "${_bw_hud_repo}/tests/hud_customization_test.c")
  target_link_libraries(bluewake_hud_core_test PRIVATE bluewake_hud_fixture_core)
  add_test(NAME bluewake_hud_core_test COMMAND bluewake_hud_core_test)
  if(TARGET GXRuntime::gxcore)
    add_executable(bluewake_hud_draw_plan_test
      "${_bw_hud_repo}/tests/hud_customization_draw_plan_test.cpp"
      "${_bw_hud_repo}/runtime/host/src/hud_customization_draw_plan.cpp")
    target_link_libraries(bluewake_hud_draw_plan_test PRIVATE bluewake_hud_fixture_core GXRuntime::gxcore)
    # The active donor header selects the real appended tint constant/type.
    # The fixture tests those actual types, not a separately declared ABI.
    add_test(NAME bluewake_hud_draw_plan_test COMMAND bluewake_hud_draw_plan_test)
    add_executable(bluewake_hud_stream_test
      "${_bw_hud_repo}/tests/hud_stream_test.cpp"
      "${_bw_hud_repo}/runtime/host/src/hud_customization_draw_plan.cpp")
    target_link_libraries(bluewake_hud_stream_test PRIVATE bluewake_hud_fixture_core GXRuntime::gxcore)
    # Actual GxCoreSink/consuming frontend ordered-packet fixture. This public
    # variant needs no backend extractor, private descriptor or renderer init.
    add_test(NAME bluewake_hud_stream_test COMMAND bluewake_hud_stream_test)
  endif()
endif()
foreach(_target bluewake_settings_catalog_test bluewake_settings_ui_test)
  if(TARGET ${_target})
    target_sources(${_target} PRIVATE "${_bw_hud_repo}/runtime/host/src/hud_customization.c")
    target_include_directories(${_target} PRIVATE "${_bw_hud_repo}/runtime/host/src")
  endif()
endforeach()
if(TARGET bluewake_settings_state_test)
  target_include_directories(bluewake_settings_state_test PRIVATE "${_bw_hud_repo}/runtime/host/src")
endif()
if(TARGET bluewake_settings_ui_test)
  target_sources(bluewake_settings_ui_test PRIVATE "${_bw_hud_repo}/tests/hud_settings_ui_mocks.cpp")
endif()
if(BUILD_TESTING AND NOT TARGET bluewake_hud_preferences_test)
  add_executable(bluewake_hud_preferences_test
    "${_bw_hud_repo}/tests/hud_preferences_test.cpp"
    "${_bw_hud_repo}/windows/src/settings_catalog.cpp"
    "${_bw_hud_repo}/windows/src/settings_presets.cpp"
    "${_bw_hud_repo}/runtime/host/src/setting_definitions.cpp"
    "${_bw_hud_repo}/runtime/host/src/hud_customization.c")
  target_include_directories(bluewake_hud_preferences_test PRIVATE
    "${_bw_hud_repo}/runtime/host/src" "${_bw_hud_repo}/windows/src")
  target_compile_features(bluewake_hud_preferences_test PRIVATE c_std_11 cxx_std_17)
  add_test(NAME bluewake_hud_preferences_test COMMAND bluewake_hud_preferences_test
    "${CMAKE_CURRENT_BINARY_DIR}/hud-preferences-fixture")
endif()
