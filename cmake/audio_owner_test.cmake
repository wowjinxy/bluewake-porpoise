# Compile the exact main.c owner reader/epoch/attachment seam in isolation.
# Real GXRuntime aliases and audio core; no window, input, sound device or assets.
set(_audio_owner_source "${BLUEWAKE_HOST_SRC}/main.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_audio_owner_source}")
file(READ "${_audio_owner_source}" _audio_owner_text)
string(FIND "${_audio_owner_text}" "static CPUState* g_audio_owner_cpu;" _audio_owner_begin)
string(FIND "${_audio_owner_text}" "// Native CARD loads can reuse" _audio_owner_end)
if(_audio_owner_begin LESS 0 OR _audio_owner_end LESS _audio_owner_begin)
    message(FATAL_ERROR "Cannot extract the production audio ownership seam")
endif()
math(EXPR _audio_owner_length "${_audio_owner_end}-${_audio_owner_begin}")
string(SUBSTRING "${_audio_owner_text}" ${_audio_owner_begin} ${_audio_owner_length} _audio_owner_functions)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/audio_owner_under_test.inc" "${_audio_owner_functions}")
add_executable(bluewake_audio_owner_test
    "${BLUEWAKE_REPO_ROOT}/tests/audio_owner_test.c"
    "${BLUEWAKE_HOST_SRC}/audio_customization.c")
target_include_directories(bluewake_audio_owner_test PRIVATE "${BLUEWAKE_HOST_SRC}" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(bluewake_audio_owner_test PRIVATE c_std_11)
target_link_libraries(bluewake_audio_owner_test PRIVATE gxruntime)
add_test(NAME bluewake_audio_owner_test COMMAND bluewake_audio_owner_test)
set_tests_properties(bluewake_audio_owner_test PROPERTIES TIMEOUT 15)
