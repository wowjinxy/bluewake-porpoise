# Copied-value native LAN transport. Deliberately independent of game/SDK headers.
find_package(Threads REQUIRED)
add_library(cos_network STATIC
    "${COS_NATIVE_ROOT}/src/pc/network/network_config.cpp"
    "${COS_NATIVE_ROOT}/src/pc/network/network_session.cpp"
    "${COS_NATIVE_ROOT}/src/pc/network/network_store.cpp")
set_target_properties(cos_network PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_include_directories(cos_network PUBLIC "${COS_NATIVE_ROOT}/include"
    PRIVATE "${COS_NATIVE_ROOT}/src/pc/network")
target_link_libraries(cos_network PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(cos_network PUBLIC ws2_32)
endif()

foreach(kind IN ITEMS schema transport store)
    add_executable(cos_network_${kind}_tests "${COS_NATIVE_ROOT}/check/pc_network_${kind}_test.cpp")
    set_target_properties(cos_network_${kind}_tests PROPERTIES CXX_STANDARD 17)
    target_include_directories(cos_network_${kind}_tests PRIVATE "${COS_NATIVE_ROOT}/src/pc/network")
    target_link_libraries(cos_network_${kind}_tests PRIVATE cos_network)
endforeach()
add_executable(cos_network_server "${COS_NATIVE_ROOT}/check/pc_network_server.cpp")
set_target_properties(cos_network_server PROPERTIES CXX_STANDARD 17)
target_link_libraries(cos_network_server PRIVATE cos_network)
add_executable(cos_network_profile_tests "${COS_NATIVE_ROOT}/check/pc_network_profile_test.cpp"
    "${COS_NATIVE_ROOT}/src/pc/features/pc_network_profile.cpp")
target_include_directories(cos_network_profile_tests PRIVATE "${COS_NATIVE_ROOT}/src/pc/network")
target_link_libraries(cos_network_profile_tests PRIVATE cos_network)
add_executable(cos_progression_logic_tests "${COS_NATIVE_ROOT}/check/pc_progression_logic_test.cpp")
target_include_directories(cos_progression_logic_tests PRIVATE "${COS_NATIVE_ROOT}/include")
