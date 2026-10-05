# Production network menu/preferences/digest + actual offscreen ImGui widgets.
# Only its game command/snapshot C ABI is mocked; no game, sockets, backend,
# native window, renderer, SDL initialization or desktop input is used.
get_target_property(_bw_network_ui_imgui_sources imgui SOURCES)
list(GET _bw_network_ui_imgui_sources 0 _bw_network_ui_imgui_first)
get_filename_component(_bw_network_ui_imgui_dir "${_bw_network_ui_imgui_first}" DIRECTORY)
get_target_property(_bw_network_ui_imgui_definitions imgui INTERFACE_COMPILE_DEFINITIONS)
add_executable(bluewake_network_menu_test
    ${BLUEWAKE_REPO_ROOT}/tests/network_menu_test.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/network_menu.cpp
    ${BLUEWAKE_HOST_SRC}/network_preferences.cpp
    ${BLUEWAKE_HOST_SRC}/health_host.c
    ${BLUEWAKE_HOST_SRC}/health_rules.c
    ${BLUEWAKE_REPO_ROOT}/tests/health_network_fixture.c
    ${BLUEWAKE_REPO_ROOT}/windows/resources/BlueWake.manifest
    ${_bw_network_ui_imgui_dir}/imgui.cpp
    ${_bw_network_ui_imgui_dir}/imgui_draw.cpp
    ${_bw_network_ui_imgui_dir}/imgui_tables.cpp
    ${_bw_network_ui_imgui_dir}/imgui_widgets.cpp)
target_compile_features(bluewake_network_menu_test PRIVATE cxx_std_17)
target_compile_definitions(bluewake_network_menu_test PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX IMGUI_ENABLE_TEST_ENGINE ${_bw_network_ui_imgui_definitions})
target_compile_options(bluewake_network_menu_test PRIVATE -UNDEBUG)
target_include_directories(bluewake_network_menu_test PRIVATE
    ${BLUEWAKE_REPO_ROOT}/windows/src ${BLUEWAKE_HOST_SRC}
    ${GXRUNTIME_DIR}/include ${GXRUNTIME_DIR}/graphics/aurora/include
    ${BLUEWAKE_RECOMPCORE}/Source/Core/Core/PowerPC/StaticRecomp
    ${BLUEWAKE_COMPAT_INCLUDES} ${_bw_network_ui_imgui_dir}
    $<TARGET_PROPERTY:${AURORA_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(bluewake_network_menu_test PRIVATE gxruntime bcrypt ws2_32)
if("IMGUI_ENABLE_FREETYPE" IN_LIST _bw_network_ui_imgui_definitions)
    target_sources(bluewake_network_menu_test PRIVATE ${_bw_network_ui_imgui_dir}/misc/freetype/imgui_freetype.cpp)
    target_link_libraries(bluewake_network_menu_test PRIVATE Freetype::Freetype)
endif()
if(TARGET zlib)
    add_custom_command(TARGET bluewake_network_menu_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:zlib>" "$<TARGET_FILE_DIR:bluewake_network_menu_test>")
endif()
# prepare() is deliberately process-once; each launch exercises its genuine
# mounted mode and can never reset that production static context in a test.
foreach(_bw_network_mode personal room mods-on mods-off)
    add_test(NAME bluewake_network_menu_test_${_bw_network_mode}
        COMMAND bluewake_network_menu_test ${_bw_network_mode} ${CMAKE_CURRENT_BINARY_DIR}/network-menu-fixture)
    set_tests_properties(bluewake_network_menu_test_${_bw_network_mode} PROPERTIES TIMEOUT 30)
endforeach()
