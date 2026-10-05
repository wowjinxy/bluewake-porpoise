# Source-only synthetic fixture. Production main seams are extracted read-only
# into this build directory; no ROM/module/state/card input is needed.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(_song_seams "${CMAKE_CURRENT_BINARY_DIR}/song-host-seams")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${BLUEWAKE_HOST_SRC}/main.c" "${BLUEWAKE_REPO_ROOT}/tests/song_host_adapter_prepare.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${BLUEWAKE_REPO_ROOT}/tests/song_host_adapter_prepare.py"
    --main "${BLUEWAKE_HOST_SRC}/main.c" --output "${_song_seams}"
    RESULT_VARIABLE _song_prepared ERROR_VARIABLE _song_error)
if(NOT _song_prepared EQUAL 0)
    message(FATAL_ERROR "Cannot extract current song host fixture seams: ${_song_error}")
endif()
add_executable(bluewake_song_host_adapter_test
    "${BLUEWAKE_REPO_ROOT}/tests/song_host_adapter_test.c"
    "${BLUEWAKE_HOST_SRC}/song_host_adapter.c"
    "${BLUEWAKE_HOST_SRC}/song_rel_owner.c"
    "${BLUEWAKE_HOST_SRC}/game_events.c"
    "${BLUEWAKE_HOST_SRC}/rel_scratch_allocator.c")
target_include_directories(bluewake_song_host_adapter_test PRIVATE
    "${_song_seams}" "${BLUEWAKE_HOST_SRC}" "${GXRUNTIME_DIR}/include"
    "${BLUEWAKE_REPO_ROOT}/cmake/composite"
    "${BLUEWAKE_RECOMPCORE}/Source/Core/Core/PowerPC/StaticRecomp")
target_compile_features(bluewake_song_host_adapter_test PRIVATE c_std_11)
target_compile_definitions(bluewake_song_host_adapter_test PRIVATE BLUEWAKE_DIRECT_CALLS=1 _CRT_SECURE_NO_WARNINGS)
target_link_libraries(bluewake_song_host_adapter_test PRIVATE gxruntime)
# Optional own-disc optimized-caller qualification; absent by default. This
# path must remain local/ignored and must never be added to a public artifact.
if(BLUEWAKE_PRIVATE_SONG_CALL_INCLUDE)
    if(NOT EXISTS "${BLUEWAKE_PRIVATE_SONG_CALL_INCLUDE}")
        message(FATAL_ERROR "Private Hr caller include was explicitly selected but is missing")
    endif()
    target_compile_definitions(bluewake_song_host_adapter_test PRIVATE
        BLUEWAKE_PRIVATE_HR_CALL_INCLUDE="${BLUEWAKE_PRIVATE_SONG_CALL_INCLUDE}")
endif()
add_test(NAME bluewake_song_host_adapter_test COMMAND bluewake_song_host_adapter_test)
set_tests_properties(bluewake_song_host_adapter_test PROPERTIES TIMEOUT 60)
