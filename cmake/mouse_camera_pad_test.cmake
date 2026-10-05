# Compile the production block/PAD functions with synthetic camera context and
# stick samples. No SDL window, native input, renderer or game assets are used.
set(_mouse_pad_source "${BLUEWAKE_HOST_SRC}/mouse_camera.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_mouse_pad_source}")
file(READ "${_mouse_pad_source}" _mouse_pad_source_text)

function(_bluewake_extract_mouse_camera output begin_marker end_marker)
    string(FIND "${_mouse_pad_source_text}" "${begin_marker}" _begin)
    string(FIND "${_mouse_pad_source_text}" "${end_marker}" _end)
    if(_begin LESS 0 OR _end LESS _begin)
        message(FATAL_ERROR "Could not locate ${begin_marker} for the camera PAD regression")
    endif()
    math(EXPR _length "${_end} - ${_begin}")
    string(SUBSTRING "${_mouse_pad_source_text}" ${_begin} ${_length} _function)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${output}" "${_function}")
endfunction()

_bluewake_extract_mouse_camera(mouse_camera_block_under_test.inc
    "void bluewake_mouse_camera_block(" "static bool SDLCALL discard_queued_motion(")
_bluewake_extract_mouse_camera(mouse_camera_discard_under_test.inc
    "static bool SDLCALL discard_queued_motion(" "bool bluewake_mouse_camera_scripted(")
_bluewake_extract_mouse_camera(mouse_camera_event_under_test.inc
    "static void observe(" "void bluewake_mouse_camera_install(")
_bluewake_extract_mouse_camera(mouse_camera_pad_under_test.inc
    "void bluewake_mouse_camera_pad(" "static float read_f32(")

add_executable(bluewake_mouse_camera_pad_test
    "${BLUEWAKE_REPO_ROOT}/tests/mouse_camera_pad_test.c")
target_compile_features(bluewake_mouse_camera_pad_test PRIVATE c_std_11)
target_compile_definitions(bluewake_mouse_camera_pad_test PRIVATE SDL_STATIC)
target_include_directories(bluewake_mouse_camera_pad_test PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}" "${GXRUNTIME_DIR}/include"
    $<TARGET_PROPERTY:${AURORA_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
if(NOT WIN32)
    target_link_libraries(bluewake_mouse_camera_pad_test PRIVATE m)
endif()
add_test(NAME bluewake_mouse_camera_pad_test COMMAND bluewake_mouse_camera_pad_test)
set_tests_properties(bluewake_mouse_camera_pad_test PROPERTIES LABELS controls TIMEOUT 15)
