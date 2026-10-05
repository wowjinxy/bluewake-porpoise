# Actual catalog/menu + fixture-local ImGui input. Native picker and Aurora
# startup are compiled out; real loader coverage lives in asset_packs_test.cmake.
get_target_property(_bw_pack_imgui_sources imgui SOURCES)
list(GET _bw_pack_imgui_sources 0 _bw_pack_imgui_first)
get_filename_component(_bw_pack_imgui_dir "${_bw_pack_imgui_first}" DIRECTORY)
get_target_property(_bw_pack_imgui_definitions imgui INTERFACE_COMPILE_DEFINITIONS)
add_executable(bluewake_asset_pack_menu_test
    ${BLUEWAKE_REPO_ROOT}/tests/asset_pack_menu_test.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/asset_pack_menu.cpp
    ${BLUEWAKE_HOST_SRC}/asset_packs.cpp
    ${_bw_pack_imgui_dir}/imgui.cpp
    ${_bw_pack_imgui_dir}/imgui_draw.cpp
    ${_bw_pack_imgui_dir}/imgui_tables.cpp
    ${_bw_pack_imgui_dir}/imgui_widgets.cpp)
target_compile_features(bluewake_asset_pack_menu_test PRIVATE cxx_std_20)
target_compile_definitions(bluewake_asset_pack_menu_test PRIVATE
    BLUEWAKE_ASSET_PACK_MENU_TEST=1 IMGUI_ENABLE_TEST_ENGINE ${_bw_pack_imgui_definitions})
target_compile_options(bluewake_asset_pack_menu_test PRIVATE -UNDEBUG)
target_include_directories(bluewake_asset_pack_menu_test PRIVATE
    ${BLUEWAKE_REPO_ROOT}/windows/src ${BLUEWAKE_HOST_SRC}
    ${GXRUNTIME_DIR}/graphics/aurora/include ${_bw_pack_imgui_dir}
    $<TARGET_PROPERTY:${AURORA_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(bluewake_asset_pack_menu_test PRIVATE xxhash)
if("IMGUI_ENABLE_FREETYPE" IN_LIST _bw_pack_imgui_definitions)
    target_sources(bluewake_asset_pack_menu_test PRIVATE ${_bw_pack_imgui_dir}/misc/freetype/imgui_freetype.cpp)
    target_link_libraries(bluewake_asset_pack_menu_test PRIVATE Freetype::Freetype)
endif()
add_test(NAME bluewake_asset_pack_menu_test COMMAND bluewake_asset_pack_menu_test ${CMAKE_CURRENT_BINARY_DIR}/asset-pack-menu-fixture)
set_tests_properties(bluewake_asset_pack_menu_test PROPERTIES LABELS assets TIMEOUT 60)
