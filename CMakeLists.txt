# CMakeLists.txt

cmake_minimum_required(VERSION 3.10)
project(VPU)

# Define colors
string(ASCII 27 Esc)
set(C_Reset       "${Esc}[m")
set(C_Bold        "${Esc}[1m")
set(C_Red         "${Esc}[31m")
set(C_Green       "${Esc}[32m")
set(C_Yellow      "${Esc}[33m")
set(C_Blue        "${Esc}[34m")
set(C_Magenta     "${Esc}[35m")
set(C_Cyan        "${Esc}[36m")
set(C_White       "${Esc}[37m")
set(C_BoldRed     "${Esc}[1;31m")
set(C_BoldGreen   "${Esc}[1;32m")
set(C_BoldYellow  "${Esc}[1;33m")
set(C_BoldBlue    "${Esc}[1;34m")
set(C_BoldMagenta "${Esc}[1;35m")
set(C_BoldCyan    "${Esc}[1;36m")
set(C_BoldWhite   "${Esc}[1;37m")

# Set a variable to control manual vs. automatic codec selection
set(MANUAL_BUILD_MODE OFF) # Set to ON if manually adding subdirectories
if (MANUAL_BUILD_MODE)
    # Manually add subdirectory for specific codecs
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VPU_D6")
        add_subdirectory(VPU_D6)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VPU_C7")
        add_subdirectory(VPU_C7)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JPU_C5")
        add_subdirectory(JPU_C5)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JPU_C6")
        add_subdirectory(JPU_C6)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/HEVC_D1")
        add_subdirectory(HEVC_D1)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/HEVC_D2")
        add_subdirectory(HEVC_D2)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/HEVC_E3")
        add_subdirectory(HEVC_E3)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VP9_D1")
        add_subdirectory(VP9_D1)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VPU_4K_D2")
        add_subdirectory(VPU_4K_D2)
    endif()
else()
    # Process the SELECTED_CODECS variable
    string(REPLACE "," ";" SELECTED_CODECS_LIST ${SELECTED_CODECS})

    foreach(codec ${SELECTED_CODECS_LIST})
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${codec}")
            # Check if the folder is empty
            file(GLOB FILES_IN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${codec}/*")

            if(FILES_IN_DIR)
                # If the directory contains files, add the subdirectory
                add_subdirectory(${codec})
            else()
                # If the directory is empty, print a message
                message("Skipping ${C_Green}${codec}${C_Reset}: Directory ${C_Cyan}${codec}${C_Reset} is empty!")
            endif()
        else()
            # If the directory doesn't exist, print a message
            message("Skipping ${C_Green}${codec}${C_Reset}: Directory ${C_Cyan}${codec}${C_Reset} not found!")
        endif()
    endforeach()
endif()
