# Sprint modes: pure state, actual production C boundary, gated bindings,
# real SDL virtual controllers, and local actual ImGui IO. No native game,
# SDL video/backend, physical input, window, GPU or desktop interaction.
function(_bw_sprint_test target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE AURORA TARGET_PC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${target} PRIVATE -UNDEBUG)
    target_include_directories(${target} PRIVATE ${BLUEWAKE_HOST_SRC} ${BLUEWAKE_REPO_ROOT}/tests
        ${GXRUNTIME_DIR}/graphics/aurora/include
        $<TARGET_PROPERTY:${AURORA_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
endfunction()
add_executable(bluewake_sprint_input_test ${BLUEWAKE_REPO_ROOT}/tests/sprint_input_test.c ${BLUEWAKE_HOST_SRC}/sprint_input.c)
_bw_sprint_test(bluewake_sprint_input_test)
add_test(NAME bluewake_sprint_input_test COMMAND bluewake_sprint_input_test)
add_executable(bluewake_sprint_configuration_thread_test ${BLUEWAKE_REPO_ROOT}/tests/sprint_configuration_thread_test.cpp ${BLUEWAKE_HOST_SRC}/sprint_input.c)
_bw_sprint_test(bluewake_sprint_configuration_thread_test)
add_test(NAME bluewake_sprint_configuration_thread_test COMMAND bluewake_sprint_configuration_thread_test)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BLUEWAKE_HOST_SRC}/main.c")
file(READ "${BLUEWAKE_HOST_SRC}/main.c" _bw_sprint_main)
function(_bw_sprint_extract output start finish)
    string(FIND "${_bw_sprint_main}" "${start}" _begin)
    if(_begin LESS 0)
        message(FATAL_ERROR "Missing Sprint lifetime start ${start}")
    endif()
    string(SUBSTRING "${_bw_sprint_main}" ${_begin} -1 _tail)
    string(FIND "${_tail}" "${finish}" _end)
    if(_end LESS 0)
        message(FATAL_ERROR "Missing Sprint lifetime end ${finish}")
    endif()
    string(SUBSTRING "${_tail}" 0 ${_end} _body)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${output}.inc" "${_body}")
endfunction()
_bw_sprint_extract(sprint_state_replace_under_test "    host_audio_diagnostics_report(\"state-replace\");" "    chunk = bw_state_find(&reader, \"ARAM\");")
_bw_sprint_extract(sprint_state_complete_under_test "    // What the machine derives from the state rather than holds." "    // FORCE may load")
_bw_sprint_extract(sprint_main_prelude_under_test "    // A repeated in-process invocation cannot retain a borrowed previous CPU." "    // The options menu's saved choices")
_bw_sprint_extract(sprint_scene_callback_under_test "static void host_enhancement_reset(const BwGameEvent* event, void* user) {" "/* Read-only handshake for direct calls.")
_bw_sprint_extract(sprint_scene_subscription_under_test "    g_enhancement_reset_subscription = bluewake_game_events_subscribe(" "    bluewake_game_events_attach(&cpu);")
_bw_sprint_extract(sprint_failed_load_after_reset_under_test "                bluewake_game_events_reset(NULL, BW_GAME_RESET_MODULE_RELOAD);" "                bluewake_haptics_shutdown();")



add_executable(bluewake_sprint_boundary_test ${BLUEWAKE_REPO_ROOT}/tests/sprint_boundary_test.cpp
    ${BLUEWAKE_HOST_SRC}/sprint.c ${BLUEWAKE_HOST_SRC}/sprint_input.c)
_bw_sprint_test(bluewake_sprint_boundary_test)
# This deliberately tiny fixture CPU counts every guest access. It qualifies
# the real Sprint function bodies and addresses, not the game CPU ABI.
target_include_directories(bluewake_sprint_boundary_test BEFORE PRIVATE ${BLUEWAKE_REPO_ROOT}/tests/sprint_modes_support ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(bluewake_sprint_boundary_test PRIVATE BLUEWAKE_WINDOWS=1)
add_test(NAME bluewake_sprint_boundary_test COMMAND bluewake_sprint_boundary_test)
add_executable(bluewake_sprint_actions_test ${BLUEWAKE_REPO_ROOT}/tests/sprint_actions_test.cpp ${BLUEWAKE_HOST_SRC}/controls_bindings.cpp)
_bw_sprint_test(bluewake_sprint_actions_test)
add_test(NAME bluewake_sprint_actions_test COMMAND bluewake_sprint_actions_test)
add_executable(bluewake_sprint_backend_test ${BLUEWAKE_REPO_ROOT}/tests/sprint_backend_test.cpp
    ${BLUEWAKE_HOST_SRC}/controls_bindings.cpp ${BLUEWAKE_HOST_SRC}/sprint_input.c
    ${GXRUNTIME_DIR}/graphics/aurora/lib/input.cpp ${GXRUNTIME_DIR}/graphics/aurora/lib/device.cpp
    ${GXRUNTIME_DIR}/graphics/aurora/lib/logging.cpp ${GXRUNTIME_DIR}/graphics/aurora/lib/dolphin/pad/pad.cpp)
_bw_sprint_test(bluewake_sprint_backend_test)
target_compile_definitions(bluewake_sprint_backend_test PRIVATE BLUEWAKE_WINDOWS=1)
target_include_directories(bluewake_sprint_backend_test PRIVATE ${GXRUNTIME_DIR}/graphics/aurora/lib)
target_link_libraries(bluewake_sprint_backend_test PRIVATE ${AURORA_SDL3_TARGET} fmt::fmt absl::flat_hash_map)
if(WIN32)
    add_custom_command(TARGET bluewake_sprint_backend_test POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:bluewake_sprint_backend_test> $<TARGET_FILE_DIR:bluewake_sprint_backend_test>
        COMMAND_EXPAND_LISTS VERBATIM)
endif()
add_test(NAME bluewake_sprint_backend_test COMMAND bluewake_sprint_backend_test)

get_target_property(_bw_sprint_imgui_sources imgui SOURCES)
list(GET _bw_sprint_imgui_sources 0 _bw_sprint_imgui_first)
get_filename_component(_bw_sprint_imgui_dir "${_bw_sprint_imgui_first}" DIRECTORY)
get_target_property(_bw_sprint_imgui_definitions imgui INTERFACE_COMPILE_DEFINITIONS)
add_executable(bluewake_sprint_settings_ui_test ${BLUEWAKE_REPO_ROOT}/tests/sprint_settings_ui_test.cpp
    ${BLUEWAKE_REPO_ROOT}/tests/menu_size_mocks.cpp ${BLUEWAKE_REPO_ROOT}/tests/hud_settings_ui_mocks.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/win_settings.cpp ${BLUEWAKE_REPO_ROOT}/windows/src/settings_catalog.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/settings_presets.cpp ${BLUEWAKE_HOST_SRC}/setting_definitions.cpp
    ${BLUEWAKE_HOST_SRC}/controls_bindings.cpp ${BLUEWAKE_HOST_SRC}/controls_menu.cpp
    ${BLUEWAKE_HOST_SRC}/audio_preview.cpp ${BLUEWAKE_HOST_SRC}/hud_customization.c ${BLUEWAKE_HOST_SRC}/sprint_input.c
    ${_bw_sprint_imgui_dir}/imgui.cpp ${_bw_sprint_imgui_dir}/imgui_draw.cpp
    ${_bw_sprint_imgui_dir}/imgui_widgets.cpp ${_bw_sprint_imgui_dir}/imgui_tables.cpp)
_bw_sprint_test(bluewake_sprint_settings_ui_test)
target_compile_definitions(bluewake_sprint_settings_ui_test PRIVATE BLUEWAKE_WINDOWS=1 BLUEWAKE_SETTINGS_UI_TEST=1
    IMGUI_ENABLE_TEST_ENGINE MENU_SIZE_PROJECTED=1 ${_bw_sprint_imgui_definitions})
target_compile_options(bluewake_sprint_settings_ui_test PRIVATE ${BLUEWAKE_COMPAT_FORCE_INCLUDE})
target_include_directories(bluewake_sprint_settings_ui_test PRIVATE ${BLUEWAKE_REPO_ROOT}/windows/src
    ${BLUEWAKE_COMPAT_INCLUDES} ${GXRUNTIME_DIR}/include ${_bw_sprint_imgui_dir})
target_link_libraries(bluewake_sprint_settings_ui_test PRIVATE bluewake_posix_compat user32 shell32 comdlg32)
if("IMGUI_ENABLE_FREETYPE" IN_LIST _bw_sprint_imgui_definitions)
    target_sources(bluewake_sprint_settings_ui_test PRIVATE ${_bw_sprint_imgui_dir}/misc/freetype/imgui_freetype.cpp)
    target_link_libraries(bluewake_sprint_settings_ui_test PRIVATE Freetype::Freetype)
endif()
foreach(_bw_sprint_case IN ITEMS "1920,1080,100" "320,240,200")
    string(REPLACE "," ";" _bw_sprint_values "${_bw_sprint_case}")
    list(GET _bw_sprint_values 0 _bw_sprint_w)
    list(GET _bw_sprint_values 1 _bw_sprint_h)
    list(GET _bw_sprint_values 2 _bw_sprint_percent)
    set(_bw_sprint_suffix "${_bw_sprint_w}x${_bw_sprint_h}_${_bw_sprint_percent}")
    set(_bw_sprint_dir "${CMAKE_CURRENT_BINARY_DIR}/sprint-ui-fixture/${_bw_sprint_suffix}")
    add_test(NAME bluewake_sprint_settings_ui_test_${_bw_sprint_suffix} COMMAND bluewake_sprint_settings_ui_test
        ${_bw_sprint_w} ${_bw_sprint_h} ${_bw_sprint_percent} ${_bw_sprint_dir} default)
    set_tests_properties(bluewake_sprint_settings_ui_test_${_bw_sprint_suffix} PROPERTIES FIXTURES_SETUP bw_sprint_${_bw_sprint_suffix} TIMEOUT 30)
    add_test(NAME bluewake_sprint_settings_ui_reload_${_bw_sprint_suffix} COMMAND bluewake_sprint_settings_ui_test
        ${_bw_sprint_w} ${_bw_sprint_h} ${_bw_sprint_percent} ${_bw_sprint_dir} reload)
    set_tests_properties(bluewake_sprint_settings_ui_reload_${_bw_sprint_suffix} PROPERTIES FIXTURES_REQUIRED bw_sprint_${_bw_sprint_suffix} TIMEOUT 10)
endforeach()
add_test(NAME bluewake_sprint_settings_ui_legacy COMMAND bluewake_sprint_settings_ui_test 1920 1080 100
    ${CMAKE_CURRENT_BINARY_DIR}/sprint-ui-fixture/legacy legacy-read)
foreach(_bw_sprint_test IN ITEMS bluewake_sprint_input_test bluewake_sprint_configuration_thread_test
        bluewake_sprint_boundary_test bluewake_sprint_actions_test bluewake_sprint_backend_test)
    set_tests_properties(${_bw_sprint_test} PROPERTIES LABELS controls TIMEOUT 30)
endforeach()
