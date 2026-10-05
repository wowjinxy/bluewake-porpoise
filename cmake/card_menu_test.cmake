# Actual card menu + ImGui widgets + card manager/serializer; no SDL/backend,
# native window, GPU, desktop input, file picker or personal save data.
get_target_property(_bw_card_ui_imgui_sources imgui SOURCES)
list(GET _bw_card_ui_imgui_sources 0 _bw_card_ui_imgui_first)
get_filename_component(_bw_card_ui_imgui_dir "${_bw_card_ui_imgui_first}" DIRECTORY)
get_target_property(_bw_card_ui_imgui_definitions imgui INTERFACE_COMPILE_DEFINITIONS)

add_executable(bluewake_card_menu_test
    ${BLUEWAKE_REPO_ROOT}/tests/card_menu_test.cpp
    ${BLUEWAKE_REPO_ROOT}/windows/src/card_menu.cpp
    ${BLUEWAKE_HOST_SRC}/card_manager.cpp
    ${BLUEWAKE_HOST_SRC}/card_import.cpp
    ${GXRUNTIME_DIR}/src/memory_card.c
    ${_bw_card_ui_imgui_dir}/imgui.cpp
    ${_bw_card_ui_imgui_dir}/imgui_draw.cpp
    ${_bw_card_ui_imgui_dir}/imgui_tables.cpp
    ${_bw_card_ui_imgui_dir}/imgui_widgets.cpp)
target_compile_features(bluewake_card_menu_test PRIVATE cxx_std_17)
target_compile_definitions(bluewake_card_menu_test PRIVATE
    BLUEWAKE_CARD_MENU_TEST=1 IMGUI_ENABLE_TEST_ENGINE ${_bw_card_ui_imgui_definitions})
target_compile_options(bluewake_card_menu_test PRIVATE -UNDEBUG ${BLUEWAKE_COMPAT_FORCE_INCLUDE})
target_include_directories(bluewake_card_menu_test PRIVATE
    ${BLUEWAKE_REPO_ROOT}/windows/src ${BLUEWAKE_HOST_SRC}
    ${GXRUNTIME_DIR}/include ${GXRUNTIME_DIR}/graphics/aurora/include
    ${BLUEWAKE_COMPAT_INCLUDES} ${_bw_card_ui_imgui_dir}
    $<TARGET_PROPERTY:${AURORA_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(bluewake_card_menu_test PRIVATE bluewake_posix_compat user32 comdlg32)
if("IMGUI_ENABLE_FREETYPE" IN_LIST _bw_card_ui_imgui_definitions)
    target_sources(bluewake_card_menu_test PRIVATE ${_bw_card_ui_imgui_dir}/misc/freetype/imgui_freetype.cpp)
    target_link_libraries(bluewake_card_menu_test PRIVATE Freetype::Freetype)
endif()
add_test(NAME bluewake_card_menu_test COMMAND bluewake_card_menu_test ${CMAKE_CURRENT_BINARY_DIR}/card-menu-fixture)
set_tests_properties(bluewake_card_menu_test PROPERTIES TIMEOUT 30)
