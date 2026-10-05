# Experimental read-only host integration. Default OFF adds no collector
# sources, producer state, watches, libraries or compiler definitions.
include_guard(GLOBAL)
option(BLUEWAKE_NATIVE_INVENTORY_COLLECTOR
  "Build experimental headless native inventory collector" OFF)
set(BLUEWAKE_INVENTORY_APPROVED_POLICY_SHA256 "" CACHE STRING
  "Private diagnostic build approval: SHA256 of an external module policy")
function(bluewake_enable_inventory_collector target_name)
  if(NOT BLUEWAKE_NATIVE_INVENTORY_COLLECTOR)
    return()
  endif()
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR "Collector requires an existing host target")
  endif()
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "Collector lifetime cleanup requires Clang or GNU C")
  endif()
  get_target_property(_bw_collector_wired "${target_name}" BLUEWAKE_COLLECTOR_WIRED)
  if(_bw_collector_wired)
    return()
  endif()
  if(NOT "${BLUEWAKE_INVENTORY_APPROVED_POLICY_SHA256}" STREQUAL "")
    string(LENGTH "${BLUEWAKE_INVENTORY_APPROVED_POLICY_SHA256}" _bw_policy_length)
    if(NOT _bw_policy_length EQUAL 64 OR
       NOT BLUEWAKE_INVENTORY_APPROVED_POLICY_SHA256 MATCHES "^[0-9a-f]+$")
      message(FATAL_ERROR "Collector approval must be exactly 64 lowercase hexadecimal characters")
    endif()
    target_compile_definitions("${target_name}" PRIVATE
      BW_IC_APPROVED_POLICY_SHA256="${BLUEWAKE_INVENTORY_APPROVED_POLICY_SHA256}")
  endif()
  get_filename_component(_bw_collector_repo "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  include("${_bw_collector_repo}/cmake/BlueWakeRandomizerNativeContext.cmake")
  find_package(Threads REQUIRED)
  target_sources("${target_name}" PRIVATE
    "${_bw_collector_repo}/runtime/host/src/inventory_collector.cpp"
    "${_bw_collector_repo}/runtime/host/src/inventory_collector_host.cpp"
    "${_bw_collector_repo}/runtime/host/src/loaded_code_admission.cpp"
    "${_bw_collector_repo}/runtime/host/src/loaded_code_image.cpp")
  target_compile_definitions("${target_name}" PRIVATE BW_NATIVE_INVENTORY_COLLECTOR=1)
  target_compile_features("${target_name}" PRIVATE cxx_std_17)
  target_link_libraries("${target_name}" PRIVATE
    bluewake_randomizer_native_context Threads::Threads)
  if(WIN32)
    # Same system import-library closure as the qualified Windows lease fixture.
    target_link_libraries("${target_name}" PRIVATE bcrypt uuid)
  endif()
  # No artifact/policy digest is embedded in public source. Empty approval
  # compiles safely: main refuses capture and continues ordinary module load.
  set_property(TARGET "${target_name}" PROPERTY BLUEWAKE_COLLECTOR_WIRED TRUE)
endfunction()
