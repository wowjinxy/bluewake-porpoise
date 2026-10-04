cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(bluewake_porpoise_math LANGUAGES C)
endif()

set(LIBPORPOISE_DIR "" CACHE PATH "Clean, pinned cybervisi0n/libPorpoise checkout")
if(NOT IS_DIRECTORY "${LIBPORPOISE_DIR}")
  message(FATAL_ERROR "LIBPORPOISE_DIR must name the pinned libPorpoise checkout")
endif()
get_filename_component(LIBPORPOISE_DIR "${LIBPORPOISE_DIR}" REALPATH)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

set(_porpoise_prepare "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/dependencies/prepare_libporpoise_math.py")
set(_porpoise_generated "${CMAKE_CURRENT_BINARY_DIR}/libporpoise_mtx.c")
set(_porpoise_inputs
    "${LIBPORPOISE_DIR}/src/mtx/mtx.c"
    "${LIBPORPOISE_DIR}/include/dolphin/mtx.h"
    "${LIBPORPOISE_DIR}/include/dolphin/types.h"
    "${LIBPORPOISE_DIR}/include/dolphin/vec.h"
    "${LIBPORPOISE_DIR}/include/dolphin/os/OSVersion.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_porpoise_prepare}" ${_porpoise_inputs}
    "${LIBPORPOISE_DIR}/.git/HEAD" "${LIBPORPOISE_DIR}/.git/index")

# Verification precedes every configure and build. In addition to source
# hashes, the script verifies HEAD and a clean Git index/worktree. Its output
# is unchanged when the inputs match, so the check does not recompile the TU.
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${_porpoise_prepare}"
        --source "${LIBPORPOISE_DIR}" --output "${_porpoise_generated}" --quiet
    RESULT_VARIABLE _porpoise_result
    OUTPUT_VARIABLE _porpoise_receipt ERROR_VARIABLE _porpoise_error)
if(NOT _porpoise_result EQUAL 0)
  message(FATAL_ERROR "${_porpoise_error}")
endif()
add_custom_target(bluewake_porpoise_verify
    COMMAND "${Python3_EXECUTABLE}" "${_porpoise_prepare}"
        --source "${LIBPORPOISE_DIR}" --output "${_porpoise_generated}" --quiet
    BYPRODUCTS "${_porpoise_generated}"
    VERBATIM)

add_library(bluewake_porpoise_mtx STATIC "${_porpoise_generated}")
add_dependencies(bluewake_porpoise_mtx bluewake_porpoise_verify)
target_include_directories(bluewake_porpoise_mtx PRIVATE "${LIBPORPOISE_DIR}/include")
target_compile_definitions(bluewake_porpoise_mtx PRIVATE
    LIBPORPOISE_PORT LIBPORPOISE_64BIT
    C_MTXIdentity=bluewake_porpoise_mtx_identity
    C_MTXTrans=bluewake_porpoise_mtx_trans
    C_MTXScale=bluewake_porpoise_mtx_scale)
if(WIN32)
  target_compile_definitions(bluewake_porpoise_mtx PRIVATE LIBPORPOISE_BUILD_WIN)
elseif(UNIX)
  target_compile_definitions(bluewake_porpoise_mtx PRIVATE LIBPORPOISE_BUILD_LINUX)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(bluewake_porpoise_mtx PRIVATE -ffp-contract=off -fno-fast-math)
elseif(MSVC)
  target_compile_options(bluewake_porpoise_mtx PRIVATE /fp:strict)
endif()
set_target_properties(bluewake_porpoise_mtx PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED YES C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON)
