# Deliberately not included by the host build. Opt-in standalone native HUD
# ownership/semantic metadata/math prototype; no window/GPU/game dependency.
if(NOT TARGET bluewake_hud_prototype)
  set(_bw_hud_root "${CMAKE_CURRENT_LIST_DIR}/..")
  add_library(bluewake_hud_prototype STATIC
    "${_bw_hud_root}/runtime/host/src/hud_customization.c"
    "${_bw_hud_root}/runtime/host/src/hud_customization_draw_plan.cpp")
  target_compile_features(bluewake_hud_prototype PUBLIC c_std_11 cxx_std_17)
  target_include_directories(bluewake_hud_prototype PUBLIC
    "${_bw_hud_root}/runtime/host/src"
    "${_bw_hud_root}/ref/recompcore/GXRuntime/graphics/gxcore/include")
  if(BUILD_TESTING)
    add_executable(bluewake_hud_customization_test "${_bw_hud_root}/tests/hud_customization_test.c")
    target_link_libraries(bluewake_hud_customization_test PRIVATE bluewake_hud_prototype)
    add_test(NAME bluewake_hud_customization_test COMMAND bluewake_hud_customization_test)
    add_executable(bluewake_hud_draw_plan_test "${_bw_hud_root}/tests/hud_customization_draw_plan_test.cpp")
    target_link_libraries(bluewake_hud_draw_plan_test PRIVATE bluewake_hud_prototype)
    add_test(NAME bluewake_hud_draw_plan_test COMMAND bluewake_hud_draw_plan_test)
  endif()
endif()
