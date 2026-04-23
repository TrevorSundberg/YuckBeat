if(NOT DEFINED SOURCE_PATH OR SOURCE_PATH STREQUAL "")
    message(FATAL_ERROR "SOURCE_PATH is required")
endif()

if(NOT DEFINED DESTINATION_PATH OR DESTINATION_PATH STREQUAL "")
    message(FATAL_ERROR "DESTINATION_PATH is required")
endif()

get_filename_component(destination_parent "${DESTINATION_PATH}" DIRECTORY)
file(MAKE_DIRECTORY "${destination_parent}")

if(IS_DIRECTORY "${SOURCE_PATH}")
    file(REMOVE_RECURSE "${DESTINATION_PATH}")
    file(COPY "${SOURCE_PATH}" DESTINATION "${destination_parent}")
else()
    file(COPY_FILE "${SOURCE_PATH}" "${DESTINATION_PATH}" ONLY_IF_DIFFERENT)
endif()
