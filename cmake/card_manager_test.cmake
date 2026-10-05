# File-only card manager regression; actual GXRuntime serialization/validation,
# synthetic cards, no running game, personal data, SDL or native window.
add_executable(bluewake_card_manager_test
    ${BLUEWAKE_REPO_ROOT}/tests/card_manager_test.cpp
    ${BLUEWAKE_HOST_SRC}/card_manager.cpp
    ${BLUEWAKE_HOST_SRC}/card_import.cpp
    ${GXRUNTIME_DIR}/src/memory_card.c)
target_compile_features(bluewake_card_manager_test PRIVATE cxx_std_17)
target_compile_definitions(bluewake_card_manager_test PRIVATE BLUEWAKE_CARD_MANAGER_TEST=1)
target_compile_options(bluewake_card_manager_test PRIVATE -UNDEBUG ${BLUEWAKE_COMPAT_FORCE_INCLUDE})
target_include_directories(bluewake_card_manager_test PRIVATE
    ${BLUEWAKE_HOST_SRC} ${GXRUNTIME_DIR}/include ${BLUEWAKE_COMPAT_INCLUDES})
target_link_libraries(bluewake_card_manager_test PRIVATE bluewake_posix_compat)
add_test(NAME bluewake_card_manager_test COMMAND bluewake_card_manager_test ${CMAKE_CURRENT_BINARY_DIR}/card-manager-fixture)
set_tests_properties(bluewake_card_manager_test PROPERTIES TIMEOUT 30)
