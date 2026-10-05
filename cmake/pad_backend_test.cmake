# Production input/PAD with SDL virtual devices; no GPU or game assets needed.
add_executable(bluewake_pad_backend_test
    "${BLUEWAKE_REPO_ROOT}/tests/pad_backend_test.cpp"
    "${GXRUNTIME_DIR}/graphics/aurora/lib/input.cpp"
    "${GXRUNTIME_DIR}/graphics/aurora/lib/device.cpp"
    "${GXRUNTIME_DIR}/graphics/aurora/lib/logging.cpp"
    "${GXRUNTIME_DIR}/graphics/aurora/lib/dolphin/pad/pad.cpp")
target_compile_features(bluewake_pad_backend_test PRIVATE cxx_std_20)
target_compile_definitions(bluewake_pad_backend_test PRIVATE AURORA TARGET_PC)
target_include_directories(bluewake_pad_backend_test PRIVATE
    "${GXRUNTIME_DIR}/graphics/aurora/include"
    "${GXRUNTIME_DIR}/graphics/aurora/lib")
target_link_libraries(bluewake_pad_backend_test PRIVATE
    ${AURORA_SDL3_TARGET} fmt::fmt absl::flat_hash_map)
if(WIN32)
    add_custom_command(TARGET bluewake_pad_backend_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:bluewake_pad_backend_test>
            $<TARGET_FILE_DIR:bluewake_pad_backend_test>
        COMMAND_EXPAND_LISTS VERBATIM)
endif()
add_test(NAME bluewake_pad_backend_test COMMAND bluewake_pad_backend_test)
set_tests_properties(bluewake_pad_backend_test PROPERTIES LABELS controls TIMEOUT 30)

add_executable(bluewake_controls_backend_test
    "${BLUEWAKE_REPO_ROOT}/tests/controls_backend_test.cpp"
    "${BLUEWAKE_REPO_ROOT}/runtime/host/src/controls_bindings.cpp"
    "${GXRUNTIME_DIR}/graphics/aurora/lib/input.cpp"
    "${GXRUNTIME_DIR}/graphics/aurora/lib/device.cpp"
    "${GXRUNTIME_DIR}/graphics/aurora/lib/logging.cpp"
    "${GXRUNTIME_DIR}/graphics/aurora/lib/dolphin/pad/pad.cpp")
target_compile_features(bluewake_controls_backend_test PRIVATE cxx_std_20)
target_compile_definitions(bluewake_controls_backend_test PRIVATE AURORA TARGET_PC)
target_include_directories(bluewake_controls_backend_test PRIVATE
    "${BLUEWAKE_REPO_ROOT}/runtime/host/src"
    "${GXRUNTIME_DIR}/graphics/aurora/include"
    "${GXRUNTIME_DIR}/graphics/aurora/lib")
target_link_libraries(bluewake_controls_backend_test PRIVATE
    ${AURORA_SDL3_TARGET} fmt::fmt absl::flat_hash_map)
if(WIN32)
    add_custom_command(TARGET bluewake_controls_backend_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:bluewake_controls_backend_test>
            $<TARGET_FILE_DIR:bluewake_controls_backend_test>
        COMMAND_EXPAND_LISTS VERBATIM)
endif()
add_test(NAME bluewake_controls_backend_test COMMAND bluewake_controls_backend_test)
set_tests_properties(bluewake_controls_backend_test PROPERTIES LABELS controls TIMEOUT 30)
