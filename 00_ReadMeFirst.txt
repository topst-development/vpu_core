==============================================================================
                             IMPORTANT NOTICE
==============================================================================

                       *** Please read carefully ***

------------------------------------------------------------------------------


If your build target SDKs are Linux or Android(version 13 or lower), read the file below:
	- 01_ReadMe_For_Linux_or_Android.txt
The output of this method is a static library.
If a kernel object (KO) is needed, additional steps must be performed in an environment set up for building the KO using this output.

The output of this method is a kernel object.


------------------------------------------------------------------------------


==============================================================================
                         Project Directory Structure
==============================================================================
Note: Replace `${CODEC_NAME}`, `${CHIP}` with the actual values:
   - `${CODEC_NAME}`: The name of the codec (e.g., `VPU4K_D2`)
   - `${CHIP}`: The target chip (e.g., `D3`)

Ensure that the chosen CHIP and CORE are supported. Refer to the documentation for the list of supported chipsets and cores.

The project directory is organized as follows:
├── 00_ReadMeFirst.txt                                           # Provides information about the project and how to use it
├── 01_ReadMe_For_Linux_or_Android.txt                  # Provides information about the project and how to use it
├── build.sh                                        # Script to automate the build process for various chipsets
├── CMakeLists.txt                                # Main CMake configuration file for the project
├── /bin                               # Contains compiled binaries for different codecs
│   └── ${CODEC_NAME}        # Compiled binary for the ${CODEC_NAME} codec
├── /codec_common                            # Common codec files used by all ${CODEC_NAME} directories
├── /config                                         # Configuration files for build environments
│   ├── common_config.cmake              # Common configuration settings for codecs
│   ├── common_version.cmake             # Versioning settings used across codecs
│   ├── ${CHIP}.cmake                        # Chip-specific configuration (e.g., TCC805x.cmake)
│   ├── tcc_chipset_info.txt                  # Telechips Chipset Info; also used by build.sh
│   ├── tcc_video_ip_info.txt                # Telechips Video IPs; used by build.sh
│   ├── toolchain.arm64.android.config # Toolchain configuration for Android 64bit
│   ├── toolchain.arm64.linux.config     # Toolchain configuration for Linux 64bit
│   ├── toolchain.arm.android.config    # Toolchain configuration for Android 32bit
│   ├── toolchain.arm.linux.config        # Toolchain configuration for Linux 32bit
│   └── toolchain.gcc.linux.config         # Toolchain configuration for WIN32 or GCC
└──  /${CODEC_NAME}            # Directory for ${CODEC_NAME} codec (e.g., VPU4K_D2)
     ├── ${CODEC_NAME}_ko.c   # Interface file of ${CODEC_NAME} codec
     └── /source                       # Source code for the ${CODEC_NAME} codec
        └── lib_vpu_codec           # VPU codec library
            ├── api                      # API files
            ├── tcc_vpu                # TCC VPU implementation
            │   └── bitcode          # Bitcode files for IP firmware
            └── vpuapi                # IP releated files


