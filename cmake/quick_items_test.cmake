# Native guest-memory fixture; never initializes SDL video or desktop input.
add_executable(bluewake_quick_items_test
    ${BLUEWAKE_REPO_ROOT}/tests/quick_items_test.c
    ${BLUEWAKE_REPO_ROOT}/runtime/host/src/quick_items.c)
target_include_directories(bluewake_quick_items_test PRIVATE
    ${BLUEWAKE_HOST_SRC})
target_link_libraries(bluewake_quick_items_test PRIVATE gxruntime)
set_target_properties(bluewake_quick_items_test PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME bluewake_quick_items_test COMMAND bluewake_quick_items_test)
set_tests_properties(bluewake_quick_items_test PROPERTIES TIMEOUT 30)
