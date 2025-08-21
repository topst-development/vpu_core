# TCC750x.cmake

if (CORE STREQUAL "A53")
    set(ARCHITECTURE "armv8-a")
    set(CORE_SETTING "cortex-a53")
    set(USE_ARCHITECTURE FALSE)  # Optionally disable -march to prevent conflicts
endif()
