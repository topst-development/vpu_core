# source.cmake

# Set the directory containing the source files
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib_vpu_codec)

# Define the source files for the project
set(SOURCE_FILES
    ${SOURCE_DIR}/tcc_vpu/vpu_common.c
    ${SOURCE_DIR}/tcc_vpu/vpu_core.c
    ${SOURCE_DIR}/tcc_vpu/vpu_decoder.c
    ${SOURCE_DIR}/tcc_vpu/vpu_encoder.c
    ${SOURCE_DIR}/vpuapi/vpuapi.c
    ${SOURCE_DIR}/vpuapi/vpuapifunc.c
)

# Include directories for header files
include_directories(
    ${SOURCE_DIR}/api
    ${SOURCE_DIR}/tcc_vpu
    ${SOURCE_DIR}/tcc_vpu/bitcode
    ${SOURCE_DIR}/vpuapi
)

# Add necessary compile definitions
add_definitions(-DCONFIG_${PROJECT_NAME})
add_definitions(-DARM -DTCC_ONBOARD -D__${OS}__)
message(STATUS "Added definitions: -DCONFIG_${PROJECT_NAME} -DARM -DTCC_ONBOARD -D__${OS}__")
