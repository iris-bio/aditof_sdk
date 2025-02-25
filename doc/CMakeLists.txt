cmake_minimum_required(VERSION 2.8)
project(doc)

find_program(DOXYGEN_PATH doxygen)
if (NOT DOXYGEN_PATH)
    message(FATAL_ERROR "doxygen not found!")
endif()

######################### This script variables ###############################
set(SOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../sdk/include/aditof)
set(DOXY_WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen_doc)

############################# Doc variables ###################################
set(PROJECT_NAME "Time-of-Flight-SDK")
set(DOCUMENTED_FILES "${SOURCES_DIR}/frame.h \\
                      ${SOURCES_DIR}/frame_definitions.h \\
                      ${SOURCES_DIR}/camera.h \\
                      ${SOURCES_DIR}/camera_definitions.h \\
                      ${SOURCES_DIR}/system.h \\
                      ${SOURCES_DIR}/status_definitions.h \\
                      ${CMAKE_CURRENT_SOURCE_DIR}/mainpage.dox
                      "
)

################# Generate Doxygen configuration file #########################
configure_file(
    sdk.doxy.in
    ${DOXY_WORKING_DIR}/sdk-api.doxy
)

################# Custom target to build Doxygen doc ##########################
add_custom_target(doc
    COMMAND ${DOXYGEN_PATH} sdk-api.doxy
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOXY_WORKING_DIR}/html
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/img ${DOXY_WORKING_DIR}/html/img
    WORKING_DIRECTORY ${DOXY_WORKING_DIR}
)
