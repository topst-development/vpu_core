# common_config.cmake

# Expect CHIP and CORE variables to be passed in via command line
if(NOT DEFINED CHIP OR CHIP STREQUAL "")
    message(FATAL_ERROR "CHIP must be defined. Use -DCHIP=...")
endif()

if(NOT DEFINED CORE OR CORE STREQUAL "")
    message(FATAL_ERROR "CORE must be defined. Use -DCORE=...")
endif()

message(STATUS "CHIP: ${CHIP}")
message(STATUS "CORE: ${CORE}")

# Define a flag to control whether both -march and -mcpu should be used
set(USE_ARCHITECTURE TRUE)

# Set the toolchain file based on CHIP and CORE
# input: CORE
# output: ARCHITECTURE, CORE_SETTING, USE_ARCHITECTURE
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/${CHIP}.cmake")
    include(${CMAKE_CURRENT_LIST_DIR}/${CHIP}.cmake)
else()
    message(FATAL_ERROR "Unsupported ${CHIP}. Required file ${CHIP}.cmake does not exist.")
endif()

# Detect the architecture and OS early
if(NOT DEFINED CMAKE_SYSTEM_PROCESSOR)
    if(CMAKE_SYSTEM_NAME STREQUAL "Android")
        if(${ANDROID_ABI} STREQUAL "arm64-v8a")
            set(CMAKE_SYSTEM_PROCESSOR "aarch64")
        elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
            set(CMAKE_SYSTEM_PROCESSOR "arm")
        else()
            message(FATAL_ERROR "Unsupported ANDROID_ABI: ${ANDROID_ABI}")
        endif()
    else()
        message(FATAL_ERROR "CMAKE_SYSTEM_PROCESSOR is not set and cannot be detected automatically")
    endif()
endif()

message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(ARCH "AARCH64")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7-a")
    set(ARCH "AARCH32")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armhf")
    set(ARCH "AARCH32HF")
else()
    string(TOUPPER ${CMAKE_SYSTEM_PROCESSOR} ARCH)
endif()
message(STATUS "ARCH: ${ARCH}")

message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OS "LINUX")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(OS "ANDROID")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(OS "WINDOWS")
else()
    set(OS "UNKNOWN_OS")
endif()
message(STATUS "OS: ${OS}")

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

###############################################
# Compiler flags
###############################################
if(CORE MATCHES "^A" AND ${ARCH} STREQUAL "AARCH32") # cortex-a series and 32-bit only
    if(${CORE_SETTING} STREQUAL "cortex-a76" OR ${CORE_SETTING} STREQUAL "cortex-a55")
        message(STATUS "The compiler does not support ${CORE_SETTING}. Alternative settings 'cortex-a72' will be used.")
        set(CORE_SETTING "cortex-a72")
    endif()
endif()
if(USE_ARCHITECTURE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=${ARCHITECTURE} -mcpu=${CORE_SETTING}")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=${CORE_SETTING}")
endif()
#-pie: Generates Position Independent Executable (PIE).
#-fstack-protector-strong: Strengthens stack protection to defend against stack buffer overflow attacks.
#-fPIE: Generates Position Independent Code (PIC), enhancing security by randomizing code locations during execution.
#-D_FORTIFY_SOURCE=2: Activates enhanced security features to prevent certain vulnerabilities at runtime.
#-Wformat: Enables warnings related to the use of format strings in functions like printf and scanf.
#-Wformat-security: Activates warnings for security issues related to format strings.
#-fPIC: Generates Position Independent Code (PIC), randomizing the code's location during execution, commonly used for building dynamic libraries.
#-Werror=format-security: Treats format string-related warnings as errors, causing the build to fail.
#-Werror: Converts all compiler warnings into errors. This ensures that any potential issues in the code,
#         even those that might seem minor, are treated as critical errors that must be resolved before the code can be compiled.
#         This is useful for maintaining high code quality and preventing overlooked issues from making it into production.
#-Wall: Enables most warning messages.
#-g: Compiles with debugging information.
#-fno-short-enums: Treats enum types as large integers where possible.
#-fshort-wchar: Treats wchar_t as a 2-byte type, conserving memory.
if(CHIP STREQUAL "TCC897x")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -Wformat -Wformat-security") # for TCC897x
else()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pie")
    endif()
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong -fPIE -Wformat -Wformat-security -Werror=format-security -fPIC") #default
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -fPIE -Wformat -Wformat-security") #default
endif()
if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2") #default
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror") # included -Werror=format-security
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -v") # compile message
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g") # symbol table (size big)
endif()

#######################################################################
## VPU specific cflags

# Set flags only for 32-bit ARM cores starting with 'A'
if(CORE MATCHES "^A" AND ${ARCH} STREQUAL "AARCH32") # cortex-a series and 32-bit only
    if(${ARCHITECTURE} STREQUAL "armv8-a") # ARMv8-A architecture
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -marm") # Set ARM mode for ARMv8-A in AARCH32 mode
        #-marm, Avoid this warning: IT blocks containing 32-bit Thumb instructions are deprecated in ARMv8
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon") # Add NEON support
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=soft") # Set floating-point ABI to 'soft'
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # This option is supported only by GCC
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-thumb-interwork") # Disable interworking between ARM and Thumb code
    endif()
endif()

# These flags are architecture-independent
set(CMAKE_POSITION_INDEPENDENT_CODE OFF) # If this flag is enabled,  forced enable PIC flag when builded by CLANG.

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-pic") # Prevent multiple definitions of global variables
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-PIC") # Prevent multiple definitions of global variables
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common") # Prevent multiple definitions of global variables
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftree-vectorize") # Enable automatic vectorization of loops
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-short-enums") # Use 'int' size for enums
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Winit-self") # Warn if a variable is initialized with itself
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fmessage-length=0") # Output error and warning messages without line wrapping

message(STATUS "CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
