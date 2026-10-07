add_executable(bluewake_game_events_test
    "${BLUEWAKE_REPO_ROOT}/tests/game_events_test.c"
    "${BLUEWAKE_HOST_SRC}/game_events.c")
target_include_directories(bluewake_game_events_test PRIVATE "${BLUEWAKE_HOST_SRC}")
target_compile_features(bluewake_game_events_test PRIVATE c_std_11)
target_link_libraries(bluewake_game_events_test PRIVATE gxruntime)
if(UNIX)
    target_link_libraries(bluewake_game_events_test PRIVATE m)
endif()
add_test(NAME bluewake_game_events_test COMMAND bluewake_game_events_test)
set_tests_properties(bluewake_game_events_test PROPERTIES TIMEOUT 15)

# The fixture includes the real source to compare arbitrary pending returns
# against the previous live-slot predicate, including cancellation/reentrancy.
add_executable(bluewake_game_events_pending_filter_test
    "${BLUEWAKE_REPO_ROOT}/tests/game_events_pending_filter_test.c")
target_include_directories(bluewake_game_events_pending_filter_test PRIVATE "${BLUEWAKE_HOST_SRC}")
target_compile_features(bluewake_game_events_pending_filter_test PRIVATE c_std_11)
target_link_libraries(bluewake_game_events_pending_filter_test PRIVATE gxruntime)
if(UNIX)
    target_link_libraries(bluewake_game_events_pending_filter_test PRIVATE m)
endif()
add_test(NAME bluewake_game_events_pending_filter_test COMMAND bluewake_game_events_pending_filter_test)
set_tests_properties(bluewake_game_events_pending_filter_test PROPERTIES TIMEOUT 15)
