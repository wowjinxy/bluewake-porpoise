# Local actual ImGui IO only: no native window, SDL device/backend or GPU.
get_target_property(_bw_size_imgui_sources imgui SOURCES)
list(GET _bw_size_imgui_sources 0 _bw_size_imgui_first)
get_filename_component(_bw_size_imgui_dir "${_bw_size_imgui_first}" DIRECTORY)
get_target_property(_bw_size_imgui_definitions imgui INTERFACE_COMPILE_DEFINITIONS)
add_executable(bluewake_menu_size_test
    ${BLUEWAKE_REPO_ROOT}/tests/menu_size_test.cpp
    ${BLUEWAKE_REPO_ROOT}/tests/menu_size_mocks.cpp
    ${BLUEWAKE_REPO_ROOT}/tests/hud_settings_ui_mocks.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/win_settings.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/settings_catalog.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/settings_presets.cpp
    ${BLUEWAKE_HOST_SRC}/setting_definitions.cpp
    ${BLUEWAKE_HOST_SRC}/audio_preview.cpp
    ${BLUEWAKE_HOST_SRC}/sprint_input.c
    ${BLUEWAKE_HOST_SRC}/controls_bindings.cpp
    ${BLUEWAKE_HOST_SRC}/controls_menu.cpp
    ${BLUEWAKE_HOST_SRC}/hud_customization.c
    ${_bw_size_imgui_dir}/imgui.cpp ${_bw_size_imgui_dir}/imgui_draw.cpp
    ${_bw_size_imgui_dir}/imgui_tables.cpp ${_bw_size_imgui_dir}/imgui_widgets.cpp)
target_compile_features(bluewake_menu_size_test PRIVATE cxx_std_17)
target_compile_definitions(bluewake_menu_size_test PRIVATE
    AURORA TARGET_PC BLUEWAKE_WINDOWS=1 BLUEWAKE_SETTINGS_UI_TEST=1 IMGUI_ENABLE_TEST_ENGINE
    MENU_SIZE_PROJECTED=1 BW_MENU_SIZE_PUBLIC_FIXTURE=1
    ${_bw_size_imgui_definitions})
target_compile_options(bluewake_menu_size_test PRIVATE -UNDEBUG ${BLUEWAKE_COMPAT_FORCE_INCLUDE})
target_include_directories(bluewake_menu_size_test PRIVATE
    ${BLUEWAKE_REPO_ROOT}/windows/src ${BLUEWAKE_HOST_SRC}
    ${BLUEWAKE_REPO_ROOT}/tests ${BLUEWAKE_COMPAT_INCLUDES}
    ${GXRUNTIME_DIR}/include ${GXRUNTIME_DIR}/graphics/aurora/include
    ${_bw_size_imgui_dir}
    $<TARGET_PROPERTY:${AURORA_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(bluewake_menu_size_test PRIVATE bluewake_posix_compat user32 shell32 comdlg32)
if("IMGUI_ENABLE_FREETYPE" IN_LIST _bw_size_imgui_definitions)
    target_sources(bluewake_menu_size_test PRIVATE ${_bw_size_imgui_dir}/misc/freetype/imgui_freetype.cpp)
    target_link_libraries(bluewake_menu_size_test PRIVATE Freetype::Freetype)
endif()
# Setup processes write unique case directories. Separate readers are actual
# fresh processes; fixture dependencies prevent a parallel reader/writer race.
foreach(_bw_size_case IN ITEMS "320,240,100,default" "320,240,200,default"
        "1920,1080,100,default" "3840,2160,150,default"
        "320,240,200,fallback" "320,240,200,retry")
    string(REPLACE "," ";" _bw_size_values "${_bw_size_case}")
    list(GET _bw_size_values 0 _bw_size_w)
    list(GET _bw_size_values 1 _bw_size_h)
    list(GET _bw_size_values 2 _bw_size_percent)
    list(GET _bw_size_values 3 _bw_size_mode)
    set(_bw_size_suffix "${_bw_size_w}x${_bw_size_h}_${_bw_size_percent}_${_bw_size_mode}")
    set(_bw_size_dir "${CMAKE_CURRENT_BINARY_DIR}/menu-size-fixture/${_bw_size_suffix}")
    set(_bw_size_fixture "bw_menu_size_${_bw_size_suffix}")
    add_test(NAME bluewake_menu_size_test_${_bw_size_suffix}
        COMMAND bluewake_menu_size_test ${_bw_size_w} ${_bw_size_h} ${_bw_size_percent} ${_bw_size_dir} ${_bw_size_mode})
    set_tests_properties(bluewake_menu_size_test_${_bw_size_suffix} PROPERTIES
        FIXTURES_SETUP ${_bw_size_fixture} TIMEOUT 30)
    add_test(NAME bluewake_menu_size_test_reload_${_bw_size_suffix}
        COMMAND bluewake_menu_size_test ${_bw_size_w} ${_bw_size_h} ${_bw_size_percent} ${_bw_size_dir} reload)
    set_tests_properties(bluewake_menu_size_test_reload_${_bw_size_suffix} PROPERTIES
        FIXTURES_REQUIRED ${_bw_size_fixture} TIMEOUT 10)
endforeach()
