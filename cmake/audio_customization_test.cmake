# Native mixer regression fixtures, with isolated original/fixed/gain shadows.
# The donor target may be imported by a dedicated qualification project or built
# by the Windows app. Preparing shadows never changes the active donor source.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(_audio_donor_copy "${CMAKE_CURRENT_BINARY_DIR}/audio-donor")
set(_audio_draft "${BLUEWAKE_REPO_ROOT}/patches/recompcore/drafts/audio-customization-premix.patch")
set(_audio_completion_draft "${BLUEWAKE_REPO_ROOT}/patches/recompcore/drafts/zelda-mram-partial-completion.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${BLUEWAKE_RECOMPCORE}/Source/Core/Core/HW/DSPHLE/UCodes/Zelda.cpp"
    "${BLUEWAKE_RECOMPCORE}/Source/Core/Core/HW/DSPHLE/UCodes/Zelda.h"
    "${_audio_draft}" "${_audio_completion_draft}" "${BLUEWAKE_REPO_ROOT}/tests/audio_customization_prepare.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${BLUEWAKE_REPO_ROOT}/tests/audio_customization_prepare.py"
    --source "${BLUEWAKE_RECOMPCORE}" --output "${_audio_donor_copy}" --patch "${_audio_draft}"
    RESULT_VARIABLE _audio_prepare ERROR_VARIABLE _audio_prepare_error)
if(NOT _audio_prepare EQUAL 0)
    message(FATAL_ERROR "Cannot prepare isolated audio donor fixture: ${_audio_prepare_error}")
endif()
function(_bluewake_audio_prepare path)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${BLUEWAKE_REPO_ROOT}/tests/audio_customization_prepare.py"
        --source "${BLUEWAKE_RECOMPCORE}" --output "${path}" ${ARGN}
        RESULT_VARIABLE result ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot prepare isolated completion fixture: ${error}")
    endif()
endfunction()
set(_audio_fixed "${CMAKE_CURRENT_BINARY_DIR}/audio-donor-fixed")
set(_audio_original "${CMAKE_CURRENT_BINARY_DIR}/audio-donor-original")
set(_audio_gain_fixed "${CMAKE_CURRENT_BINARY_DIR}/audio-donor-gain-fixed")
set(_audio_test_original "${CMAKE_CURRENT_BINARY_DIR}/audio-completion-original")
set(_audio_test_fixed "${CMAKE_CURRENT_BINARY_DIR}/audio-completion-fixed")
_bluewake_audio_prepare("${_audio_original}")
_bluewake_audio_prepare("${_audio_fixed}" --patch "${_audio_completion_draft}")
_bluewake_audio_prepare("${_audio_gain_fixed}" --patch "${_audio_completion_draft}" --patch "${_audio_draft}")
_bluewake_audio_prepare("${_audio_test_original}" --completion-test-api)
_bluewake_audio_prepare("${_audio_test_fixed}" --patch "${_audio_completion_draft}" --completion-test-api)
function(_bluewake_audio_fixture target donor)
    add_executable(${target}
        "${BLUEWAKE_REPO_ROOT}/tests/audio_customization_test.cpp"
        "${BLUEWAKE_HOST_SRC}/audio_customization.c"
        "${donor}/Source/Core/Core/HW/DSPHLE/UCodes/Zelda.cpp")
    target_include_directories(${target} BEFORE PRIVATE "${donor}/Source/Core" "${BLUEWAKE_HOST_SRC}")
    if(WIN32)
        # Production host C uses the forced POSIX shim; donor C++ does not.
        # Qualify that exact inclusion environment to catch private helper
        # collisions with CRT/POSIX declarations before linking the app.
        target_include_directories(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:${BLUEWAKE_REPO_ROOT}/windows/compat>"
            "$<$<COMPILE_LANGUAGE:C>:${BLUEWAKE_REPO_ROOT}/windows/compat/include>")
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${BLUEWAKE_REPO_ROOT}/windows/compat/bw_posix_compat.h>")
    endif()
    target_compile_features(${target} PRIVATE c_std_11 cxx_std_23)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE -fno-ms-compatibility -fno-strict-aliasing
            "$<$<COMPILE_LANGUAGE:CXX>:-fcomplete-member-pointers>")
    endif()
    target_link_libraries(${target} PRIVATE bluewake_dsp_adapter)
endfunction()
function(_bluewake_audio_without_gain target)
    # The shipping donor publishes this feature macro so all ABI consumers
    # agree. Original/fixed-only shadow baselines intentionally reconstruct
    # the original AddVoice signature, even beneath that PUBLIC definition.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /UBLUEWAKE_DSP_AUDIO_CUSTOMIZATION)
    else()
        target_compile_options(${target} PRIVATE -UBLUEWAKE_DSP_AUDIO_CUSTOMIZATION)
    endif()
endfunction()
_bluewake_audio_fixture(bluewake_audio_customization_test "${_audio_donor_copy}")
target_compile_definitions(bluewake_audio_customization_test PRIVATE BLUEWAKE_DSP_AUDIO_CUSTOMIZATION=1)
_bluewake_audio_fixture(bluewake_audio_native_baseline "${_audio_original}")
_bluewake_audio_without_gain(bluewake_audio_native_baseline)
target_compile_definitions(bluewake_audio_native_baseline PRIVATE BLUEWAKE_AUDIO_NATIVE_BASELINE=1)
set(_audio_golden "${CMAKE_CURRENT_BINARY_DIR}/native-audio-fixture.bin")
add_test(NAME bluewake_audio_native_baseline COMMAND bluewake_audio_native_baseline --write-native "${_audio_golden}")
set_tests_properties(bluewake_audio_native_baseline PROPERTIES FIXTURES_SETUP bluewake_audio_native TIMEOUT 30)
add_test(NAME bluewake_audio_customization_test COMMAND bluewake_audio_customization_test --compare-native "${_audio_golden}")
set_tests_properties(bluewake_audio_customization_test PROPERTIES FIXTURES_REQUIRED bluewake_audio_native TIMEOUT 30)
_bluewake_audio_fixture(bluewake_audio_native_fixed_baseline "${_audio_fixed}")
_bluewake_audio_without_gain(bluewake_audio_native_fixed_baseline)
target_compile_definitions(bluewake_audio_native_fixed_baseline PRIVATE BLUEWAKE_AUDIO_NATIVE_BASELINE=1 BLUEWAKE_DSP_COMPLETION_FIXED=1)
_bluewake_audio_fixture(bluewake_audio_customization_fixed_test "${_audio_gain_fixed}")
target_compile_definitions(bluewake_audio_customization_fixed_test PRIVATE BLUEWAKE_DSP_AUDIO_CUSTOMIZATION=1 BLUEWAKE_DSP_COMPLETION_FIXED=1)
set(_audio_fixed_golden "${CMAKE_CURRENT_BINARY_DIR}/native-audio-fixed-fixture.bin")
add_test(NAME bluewake_audio_native_fixed_baseline COMMAND bluewake_audio_native_fixed_baseline --write-native "${_audio_fixed_golden}")
set_tests_properties(bluewake_audio_native_fixed_baseline PROPERTIES FIXTURES_SETUP bluewake_audio_native_fixed TIMEOUT 30)
add_test(NAME bluewake_audio_customization_fixed_test COMMAND bluewake_audio_customization_fixed_test --compare-native "${_audio_fixed_golden}")
set_tests_properties(bluewake_audio_customization_fixed_test PROPERTIES FIXTURES_REQUIRED bluewake_audio_native_fixed TIMEOUT 30)
function(_bluewake_audio_completion_fixture target donor)
    add_executable(${target} "${BLUEWAKE_REPO_ROOT}/tests/audio_customization_completion_test.cpp"
        "${donor}/Source/Core/Core/HW/DSPHLE/UCodes/Zelda.cpp")
    target_include_directories(${target} BEFORE PRIVATE "${donor}/Source/Core" "${BLUEWAKE_HOST_SRC}")
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_compile_definitions(${target} PRIVATE BLUEWAKE_DSP_COMPLETION_TEST_API=1 NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE -fno-ms-compatibility -fno-strict-aliasing -fcomplete-member-pointers)
    endif()
    target_link_libraries(${target} PRIVATE bluewake_dsp_adapter)
    _bluewake_audio_without_gain(${target})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 30)
endfunction()
_bluewake_audio_completion_fixture(bluewake_audio_completion_original "${_audio_test_original}")
target_compile_definitions(bluewake_audio_completion_original PRIVATE BLUEWAKE_DSP_COMPLETION_EXPECT_DEFECT=1)
_bluewake_audio_completion_fixture(bluewake_audio_completion_fixed "${_audio_test_fixed}")
add_test(NAME bluewake_audio_shadow_prepare_test
    COMMAND "${Python3_EXECUTABLE}" "${BLUEWAKE_REPO_ROOT}/tests/audio_customization_prepare_test.py"
        --source "${BLUEWAKE_RECOMPCORE}" --output "${CMAKE_CURRENT_BINARY_DIR}/audio-shadow-qualification")
set_tests_properties(bluewake_audio_shadow_prepare_test PROPERTIES TIMEOUT 120)
