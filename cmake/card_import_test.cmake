# Original GameCube formats through the actual card manager and GXRuntime.
# Synthetic whole game files; no original game assets, desktop or renderer.
add_executable(bluewake_card_import_test
    ${BLUEWAKE_REPO_ROOT}/tests/card_import_test.cpp
    ${BLUEWAKE_HOST_SRC}/card_import.cpp
    ${BLUEWAKE_HOST_SRC}/card_manager.cpp
    ${GXRUNTIME_DIR}/src/memory_card.c)
target_compile_features(bluewake_card_import_test PRIVATE cxx_std_17)
target_compile_definitions(bluewake_card_import_test PRIVATE BLUEWAKE_CARD_MANAGER_TEST=1)
target_compile_options(bluewake_card_import_test PRIVATE -UNDEBUG ${BLUEWAKE_COMPAT_FORCE_INCLUDE})
target_include_directories(bluewake_card_import_test PRIVATE
    ${BLUEWAKE_HOST_SRC} ${GXRUNTIME_DIR}/include ${BLUEWAKE_COMPAT_INCLUDES})
target_link_libraries(bluewake_card_import_test PRIVATE bluewake_posix_compat)
add_test(NAME bluewake_card_import_test COMMAND bluewake_card_import_test ${CMAKE_CURRENT_BINARY_DIR}/card-import-fixture)
set_tests_properties(bluewake_card_import_test PROPERTIES TIMEOUT 30)
