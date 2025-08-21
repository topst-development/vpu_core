# HEVC_E3/CMakeLists.txt

#############################################################################
# Get the current directory name
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
cmake_minimum_required(VERSION 3.10)
project(${PROJECT_NAME})

#############################################################################
# Check supported chipset/core
set(SUPPORTED_CHIPSETS "TCC750x" "TCC805x" "TCC807x")
set(SUPPORTED_CORES "A72" "A53" "A76" "A55")

if(NOT (CHIP IN_LIST SUPPORTED_CHIPSETS AND CORE IN_LIST SUPPORTED_CORES))
    message("Skipping ${C_Green}${PROJECT_NAME}${C_Reset}: ${C_Cyan}${CHIP}${C_Reset}-${C_Cyan}${CORE}${C_Reset} is not supported.")
    return()
endif()

message("Starting ${C_Green}${PROJECT_NAME}${C_Reset}: ${C_Cyan}${CHIP}${C_Reset}-${C_Cyan}${CORE}${C_Reset} ...")

#############################################################################
# Configuration...

# Include the config settings based on CHIP and CORE
include(${CMAKE_CURRENT_SOURCE_DIR}/../config/common_config.cmake)

# Define the library version
# Include the version extraction config
include("${CMAKE_CURRENT_SOURCE_DIR}/../config/common_version.cmake")

# Set the path to the version header file
set(VERSION_HEADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/source/lib_vpu_codec/tcc_vpu/wave420l_version.h")

# Extract the version components
extract_define_value("${VERSION_HEADER_PATH}" "VERSION_MAJOR_CODE" CODEC_VERSION_MAJOR)
extract_define_value("${VERSION_HEADER_PATH}" "VERSION_API_CODE" CODEC_VERSION_API)
extract_define_value("${VERSION_HEADER_PATH}" "VERSION_MINOR_CODE" CODEC_VERSION_MINOR)
extract_define_value("${VERSION_HEADER_PATH}" "VERSION_FIX_CODE" CODEC_VERSION_FIX)

# Combine the extracted values to form the full version string
set(CODEC_VERSION_STR "${CODEC_VERSION_MAJOR}.${CODEC_VERSION_API}.${CODEC_VERSION_MINOR}.${CODEC_VERSION_FIX}")
message(STATUS "Extracted Version: ${CODEC_VERSION_STR}")

# Define the library name
string(SUBSTRING ${ARCHITECTURE} 0 3 ARCHITECTURE_CLEAN)
string(SUBSTRING ${ARCHITECTURE} 3 2 ARCHITECTURE_VER)
# Convert the architecture string to uppercase
string(TOUPPER "${ARCHITECTURE_CLEAN}" ARCHITECTURE_UPPER)
set(ARCHITECTURE_FINAL "${ARCHITECTURE_UPPER}${ARCHITECTURE_VER}")
# Use the final architecture in the library name
set(VPU_LIB_NAME "${CHIP}_${PROJECT_NAME}_${ARCHITECTURE_FINAL}_${ARCH}_${OS}_TOPST_V${CODEC_VERSION_STR}")
message(STATUS "VPU_LIB_NAME: ${VPU_LIB_NAME}")

# Add the source directory
add_subdirectory(source)
