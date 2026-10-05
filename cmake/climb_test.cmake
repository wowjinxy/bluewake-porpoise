add_executable(bluewake_climb_test
    "${BLUEWAKE_REPO_ROOT}/tests/climb_test.c"
    "${BLUEWAKE_HOST_SRC}/climb.c")
target_include_directories(bluewake_climb_test PRIVATE "${BLUEWAKE_HOST_SRC}")
target_compile_features(bluewake_climb_test PRIVATE c_std_11)
target_link_libraries(bluewake_climb_test PRIVATE gxruntime)
if(UNIX)
    target_link_libraries(bluewake_climb_test PRIVATE m)
endif()
add_test(NAME bluewake_climb_test COMMAND bluewake_climb_test)
set_tests_properties(bluewake_climb_test PROPERTIES TIMEOUT 15)
