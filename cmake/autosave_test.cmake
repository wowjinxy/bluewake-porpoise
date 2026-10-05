# Isolated native-phase simulation; no UI, renderer, card files or game launch.
add_executable(bluewake_autosave_test
    ${BLUEWAKE_REPO_ROOT}/tests/autosave_test.c
    ${BLUEWAKE_REPO_ROOT}/runtime/host/src/autosave.c
    ${BLUEWAKE_REPO_ROOT}/runtime/host/src/fpu_context.c)
target_include_directories(bluewake_autosave_test PRIVATE ${BLUEWAKE_HOST_SRC})
target_link_libraries(bluewake_autosave_test PRIVATE gxruntime)
set_target_properties(bluewake_autosave_test PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME bluewake_autosave_test COMMAND bluewake_autosave_test)
set_tests_properties(bluewake_autosave_test PROPERTIES TIMEOUT 30)
