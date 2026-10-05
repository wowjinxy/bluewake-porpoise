add_executable(bluewake_asset_packs_test
    "${BLUEWAKE_REPO_ROOT}/tests/asset_packs_test.cpp"
    "${BLUEWAKE_HOST_SRC}/asset_packs.cpp")
target_compile_features(bluewake_asset_packs_test PRIVATE cxx_std_20)
target_include_directories(bluewake_asset_packs_test PRIVATE "${BLUEWAKE_HOST_SRC}")
target_link_libraries(bluewake_asset_packs_test PRIVATE xxhash)
add_test(NAME bluewake_asset_packs_test COMMAND bluewake_asset_packs_test "${CMAKE_CURRENT_BINARY_DIR}/asset-pack-fixture")
set_tests_properties(bluewake_asset_packs_test PROPERTIES LABELS assets TIMEOUT 60)

if(TARGET GXRuntime::aurora)
    add_executable(bluewake_asset_packs_aurora_test
        "${BLUEWAKE_REPO_ROOT}/tests/asset_packs_aurora_test.cpp"
        "${BLUEWAKE_HOST_SRC}/asset_packs.cpp"
        "${BLUEWAKE_HOST_SRC}/asset_packs_aurora.cpp")
    target_compile_features(bluewake_asset_packs_aurora_test PRIVATE cxx_std_20)
    target_include_directories(bluewake_asset_packs_aurora_test PRIVATE "${BLUEWAKE_HOST_SRC}")
    target_link_libraries(bluewake_asset_packs_aurora_test PRIVATE GXRuntime::aurora xxhash)
    add_test(NAME bluewake_asset_packs_aurora_test COMMAND bluewake_asset_packs_aurora_test "${CMAKE_CURRENT_BINARY_DIR}/asset-pack-loader-fixture")
    set_tests_properties(bluewake_asset_packs_aurora_test PROPERTIES LABELS assets TIMEOUT 60)
endif()
