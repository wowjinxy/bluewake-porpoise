# Actual ImGui settings regression with local IO only: no SDL library/backend,
# video initialization, native window, GPU, desktop input or screenshots.
get_target_property(_bw_ui_imgui_sources imgui SOURCES)
list(GET _bw_ui_imgui_sources 0 _bw_ui_imgui_first)
get_filename_component(_bw_ui_imgui_dir "${_bw_ui_imgui_first}" DIRECTORY)
get_target_property(_bw_ui_imgui_definitions imgui INTERFACE_COMPILE_DEFINITIONS)

add_executable(bluewake_settings_ui_test
    ${BLUEWAKE_REPO_ROOT}/tests/settings_ui_test.cpp
    ${BLUEWAKE_REPO_ROOT}/tests/settings_ui_mocks.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/win_settings.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/settings_catalog.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/settings_presets.cpp
    ${BLUEWAKE_HOST_SRC}/setting_definitions.cpp
    ${BLUEWAKE_HOST_SRC}/audio_preview.cpp
    ${BLUEWAKE_HOST_SRC}/sprint_input.c
    ${BLUEWAKE_HOST_SRC}/controls_bindings.cpp
    ${BLUEWAKE_HOST_SRC}/controls_menu.cpp
    ${_bw_ui_imgui_dir}/imgui.cpp
    ${_bw_ui_imgui_dir}/imgui_draw.cpp
    ${_bw_ui_imgui_dir}/imgui_tables.cpp
    ${_bw_ui_imgui_dir}/imgui_widgets.cpp)
target_compile_features(bluewake_settings_ui_test PRIVATE cxx_std_17)
target_compile_definitions(bluewake_settings_ui_test PRIVATE
    AURORA TARGET_PC BLUEWAKE_WINDOWS=1 BLUEWAKE_SETTINGS_UI_TEST=1 IMGUI_ENABLE_TEST_ENGINE
    ${_bw_ui_imgui_definitions})
target_compile_options(bluewake_settings_ui_test PRIVATE -UNDEBUG ${BLUEWAKE_COMPAT_FORCE_INCLUDE})
target_include_directories(bluewake_settings_ui_test PRIVATE
    ${BLUEWAKE_REPO_ROOT}/windows/src ${BLUEWAKE_HOST_SRC}
    ${BLUEWAKE_REPO_ROOT}/tests ${BLUEWAKE_COMPAT_INCLUDES}
    ${GXRUNTIME_DIR}/include ${GXRUNTIME_DIR}/graphics/aurora/include
    ${_bw_ui_imgui_dir}
    $<TARGET_PROPERTY:${AURORA_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(bluewake_settings_ui_test PRIVATE bluewake_posix_compat user32 shell32 comdlg32)
if("IMGUI_ENABLE_FREETYPE" IN_LIST _bw_ui_imgui_definitions)
    target_sources(bluewake_settings_ui_test PRIVATE ${_bw_ui_imgui_dir}/misc/freetype/imgui_freetype.cpp)
    target_link_libraries(bluewake_settings_ui_test PRIVATE Freetype::Freetype)
endif()
add_test(NAME bluewake_settings_ui_test COMMAND bluewake_settings_ui_test ${CMAKE_CURRENT_BINARY_DIR}/settings-ui-fixture)
set_tests_properties(bluewake_settings_ui_test PROPERTIES TIMEOUT 30)
