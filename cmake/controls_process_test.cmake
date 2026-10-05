# CTest launches the writer and reader as separate processes with explicit argv.
# Serial identities are deterministic stubs; real SDL reconnect is covered by
# controls_backend_test. Neither fixture opens SDL video or reads native input.
add_executable(bluewake_controls_persistence_test
    "${BLUEWAKE_REPO_ROOT}/tests/controls_process_test.cpp"
    "${BLUEWAKE_HOST_SRC}/controls_bindings.cpp")
target_compile_features(bluewake_controls_persistence_test PRIVATE cxx_std_17)
target_compile_definitions(bluewake_controls_persistence_test PRIVATE AURORA TARGET_PC)
target_include_directories(bluewake_controls_persistence_test PRIVATE
    "${BLUEWAKE_HOST_SRC}" "${GXRUNTIME_DIR}/graphics/aurora/include"
    $<TARGET_PROPERTY:${AURORA_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)

set(_controls_process_dir "${CMAKE_CURRENT_BINARY_DIR}/controls_process_fixture")
add_test(NAME bluewake_controls_persistence_write_test
    COMMAND bluewake_controls_persistence_test write "${_controls_process_dir}")
add_test(NAME bluewake_controls_persistence_test
    COMMAND bluewake_controls_persistence_test read "${_controls_process_dir}")
add_test(NAME bluewake_controls_persistence_cleanup_test
    COMMAND bluewake_controls_persistence_test cleanup "${_controls_process_dir}")
set_tests_properties(bluewake_controls_persistence_write_test PROPERTIES
    FIXTURES_SETUP bluewake_controls_saved_process)
set_tests_properties(bluewake_controls_persistence_test PROPERTIES
    FIXTURES_REQUIRED bluewake_controls_saved_process)
set_tests_properties(bluewake_controls_persistence_cleanup_test PROPERTIES
    FIXTURES_CLEANUP bluewake_controls_saved_process)
set_tests_properties(bluewake_controls_persistence_write_test
    bluewake_controls_persistence_test bluewake_controls_persistence_cleanup_test PROPERTIES
    LABELS controls TIMEOUT 15)
