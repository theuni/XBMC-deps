message(STATUS "=============================================================================")
message(STATUS "CTEST_FULL_OUTPUT (Avoid ctest truncation of output)")
message(STATUS "")

if(NOT CPackComponentsForAll_BINARY_DIR)
  message(FATAL_ERROR "CPackComponentsForAll_BINARY_DIR not set")
endif(NOT CPackComponentsForAll_BINARY_DIR)

if(NOT CPackGen)
  message(FATAL_ERROR "CPackGen not set")
endif(NOT CPackGen)
get_filename_component(CPACK_LOCATION ${CMAKE_COMMAND} PATH)
set(CPackCommand "${CPACK_LOCATION}/cpack")
message("cpack = ${CPackCommand}")
if(NOT CPackCommand)
  message(FATAL_ERROR "CPackCommand not set")
endif(NOT CPackCommand)

if(NOT CPackComponentWay)
  message(FATAL_ERROR "CPackComponentWay not set")
endif(NOT CPackComponentWay)

set(expected_file_mask "")
# The usual default behavior is to expect a single file
# Then some specific generators (Archive, RPM, ...)
# May produce several numbers of files depending on
# CPACK_COMPONENT_xxx values
set(expected_count 1)
set(config_type $ENV{CMAKE_CONFIG_TYPE})
set(config_args )
if(config_type)
  set(config_args -C ${config_type})
endif()
message(" ${config_args}")
execute_process(COMMAND ${CPackCommand} -G ${CPackGen} ${config_args}
    RESULT_VARIABLE CPack_result
    OUTPUT_VARIABLE CPack_output
    ERROR_VARIABLE CPack_error
    WORKING_DIRECTORY ${CPackComponentsForAll_BINARY_DIR})

if (CPack_result)
  message(FATAL_ERROR "error: CPack execution went wrong!, CPack_output=${CPack_output}, CPack_error=${CPack_error}")
else (CPack_result)
  message(STATUS "CPack_output=${CPack_output}")
endif(CPack_result)

if(CPackGen MATCHES "ZIP")
    set(expected_file_mask "${CPackComponentsForAll_BINARY_DIR}/MyLib-*.zip")
    if (${CPackComponentWay} STREQUAL "default")
        set(expected_count 1)
    endif(${CPackComponentWay} STREQUAL "default")
    if (${CPackComponentWay} STREQUAL "OnePackPerGroup")
        set(expected_count 2)
    endif (${CPackComponentWay} STREQUAL "OnePackPerGroup")
    if (${CPackComponentWay} STREQUAL "IgnoreGroup")
        set(expected_count 4)
    endif (${CPackComponentWay} STREQUAL "IgnoreGroup")
    if (${CPackComponentWay} STREQUAL "AllInOne")
        set(expected_count 1)
    endif (${CPackComponentWay} STREQUAL "AllInOne")
    if (${CPackComponentWay} STREQUAL "AllGroupsInOne")
        set(expected_count 1)
    endif (${CPackComponentWay} STREQUAL "AllGroupsInOne")
endif(CPackGen MATCHES "ZIP")

# Now verify if the number of expected file is OK
# - using expected_file_mask and
# - expected_count
if(expected_file_mask)
  file(GLOB expected_file "${expected_file_mask}")

  message(STATUS "expected_count='${expected_count}'")
  message(STATUS "expected_file='${expected_file}'")
  message(STATUS "expected_file_mask='${expected_file_mask}'")

  if(NOT expected_file)
    message(FATAL_ERROR "error: expected_file=${expected_file} does not exist: CPackComponentsForAll test fails. (CPack_output=${CPack_output}, CPack_error=${CPack_error}")
  endif(NOT expected_file)

  list(LENGTH expected_file actual_count)
  message(STATUS "actual_count='${actual_count}'")
  if(NOT actual_count EQUAL expected_count)
    message(FATAL_ERROR "error: expected_count=${expected_count} does not match actual_count=${actual_count}: CPackComponents test fails. (CPack_output=${CPack_output}, CPack_error=${CPack_error})")
  endif(NOT actual_count EQUAL expected_count)
endif(expected_file_mask)
