# Copied synthetic DOLCARD1/native payload bytes only. No guest, assets, files,
# native CARD backend, UI, devices or save-completion authority.
include_guard(GLOBAL)
get_filename_component(_bw_card_payload_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT TARGET bluewake_randomizer_card_payload_test)
  add_executable(bluewake_randomizer_card_payload_test
    "${_bw_card_payload_repo}/tests/randomizer_card_payload_test.cpp"
    "${_bw_card_payload_repo}/runtime/host/src/randomizer_card_payload.cpp")
  target_compile_features(bluewake_randomizer_card_payload_test PRIVATE cxx_std_17)
  set_target_properties(bluewake_randomizer_card_payload_test PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
  target_include_directories(bluewake_randomizer_card_payload_test PRIVATE
    "${_bw_card_payload_repo}/runtime/host/src")
  if(WIN32)
    target_compile_definitions(bluewake_randomizer_card_payload_test PRIVATE
      _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
  endif()
  if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(bluewake_randomizer_card_payload_test PRIVATE /UNDEBUG)
  else()
    target_compile_options(bluewake_randomizer_card_payload_test PRIVATE -UNDEBUG)
  endif()
  add_test(NAME bluewake_randomizer_card_payload_test COMMAND bluewake_randomizer_card_payload_test)
  set_tests_properties(bluewake_randomizer_card_payload_test PROPERTIES
    TIMEOUT 30 LABELS "source-only;randomizer;card-payload")
endif()
