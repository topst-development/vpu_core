# source/CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
# Get the parent directory name
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(PROJECT_NAME ${PARENT_DIR} NAME)
# Set the project name to the parent directory name
project(${PROJECT_NAME})
#message(STATUS "${PROJECT_NAME}")

###################################################################
##### SET DIRECTORIES AND SOURCES
# Include source file lists from another file ${SOURCE_FILES}
include(${CMAKE_CURRENT_SOURCE_DIR}/source.cmake)

###################################################################
##### SET LIBRARY OUTPUT DIRECTORY
# Set output directory for the library
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../bin)

###################################################################
##### ADD LIBRARY
# Create a static library using the sources
add_library(${VPU_LIB_NAME} STATIC ${SOURCE_FILES})

###################################################################
#### Copy API Headers
# Define the path to the api folder and bin folder
set(API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib_vpu_codec/api")
set(BIN_DIR "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${PROJECT_NAME}")

# Add a post-build command to copy the header files
add_custom_command(TARGET ${VPU_LIB_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${API_DIR}"
    "${BIN_DIR}"
    COMMENT "Copying header files from ${API_DIR} to ${BIN_DIR}"
)
