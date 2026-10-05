# Actual copied-output seam, decoder and worker fixtures with synthetic WAVs.
# No game code/data, SDL backend, audio device, window or desktop input.
get_filename_component(_bw_preview_repo "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(BUILD_TESTING AND NOT TARGET bluewake_audio_preview_test)
  find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
  set(_bw_preview_seams "${CMAKE_CURRENT_BINARY_DIR}/audio-preview-seams")
  add_custom_command(
    OUTPUT "${_bw_preview_seams}/copy_original.inc" "${_bw_preview_seams}/copy_projected.inc"
    COMMAND "${Python3_EXECUTABLE}" "${_bw_preview_repo}/tests/audio_preview_prepare.py"
      --main "${_bw_preview_repo}/runtime/host/src/main.c" --output "${_bw_preview_seams}"
    DEPENDS "${_bw_preview_repo}/tests/audio_preview_prepare.py" "${_bw_preview_repo}/runtime/host/src/main.c"
    VERBATIM)
  add_executable(bluewake_audio_preview_test
    "${_bw_preview_repo}/runtime/host/src/audio_preview.cpp"
    "${_bw_preview_repo}/runtime/host/src/audio_customization.c"
    "${_bw_preview_repo}/tests/audio_preview_core_test.cpp"
    "${_bw_preview_repo}/tests/audio_preview_copy_test.cpp"
    "${_bw_preview_seams}/copy_original.inc" "${_bw_preview_seams}/copy_projected.inc")
  target_compile_features(bluewake_audio_preview_test PRIVATE c_std_11 cxx_std_17)
  target_compile_definitions(bluewake_audio_preview_test PRIVATE
    BLUEWAKE_WINDOWS=1 BLUEWAKE_AUDIO_PREVIEW_TEST=1 BLUEWAKE_DSP_AUDIO_CUSTOMIZATION=1)
  target_compile_options(bluewake_audio_preview_test PRIVATE -UNDEBUG ${BLUEWAKE_COMPAT_FORCE_INCLUDE})
  target_include_directories(bluewake_audio_preview_test PRIVATE
    "${_bw_preview_repo}/runtime/host/src" "${_bw_preview_repo}/tests" "${_bw_preview_seams}"
    ${BLUEWAKE_COMPAT_INCLUDES} "${GXRUNTIME_DIR}/include")
  foreach(_mode core seam threads)
    add_test(NAME bluewake_audio_preview_${_mode}_test
      COMMAND bluewake_audio_preview_test "${_mode}" "${CMAKE_CURRENT_BINARY_DIR}/preview-fixture-${_mode}")
    set_tests_properties(bluewake_audio_preview_${_mode}_test PROPERTIES TIMEOUT 30)
  endforeach()
endif()
