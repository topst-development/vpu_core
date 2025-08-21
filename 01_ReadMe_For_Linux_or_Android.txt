==============================================================================
                             IMPORTANT NOTICE
==============================================================================

                       *** Please read carefully ***

------------------------------------------------------------------------------
If you make any modifications to the files in this folder:

    - You must build ALL codecs
    - Ensure that there are NO errors
------------------------------------------------------------------------------

Failure to follow these instructions may result in incomplete or faulty builds.



==============================================================================
                              HOW TO BUILD
==============================================================================

There are two methods to build the project:

1. **Using the `build.sh` Script**

   - Run the `build.sh` script to automate the build process for the desired chipset.
   - The script will handle the selection and configuration of the chipset and build the required components.
   - Ensure all dependencies and configurations are set properly before running the script.

2. **Manual Build for an Individual Component**

   If you prefer to build an individual component, such as `${CODEC_NAME}`, please follow these steps:

   2.1. Navigate to the desired codec directory:
      cd ${CODEC_NAME}

   2.2. Create a build directory and navigate into it:
      mkdir -p tmp
      cd tmp

   2.3. Configure the build with CMake, specifying the appropriate chip and core:
      cmake \
      -DCHIP=${CHIP} -DCORE=${CORE} \
      -DCMAKE_TOOLCHAIN_FILE=../../config/toolchain.arm.linux.config \
      -DOS=Linux \
      -DCMAKE_BUILD_TYPE=Release \
      ..

   Note: Replace `${CODEC_NAME}`, `${CHIP}`, and `${CORE}` with the actual values:
   - `${CODEC_NAME}`: The name of the codec (e.g., `VPU_C7`)
   - `${CHIP}`: The target chip (e.g., `D3`)
   - `${CORE}`: The core (e.g., `A72`)

   Ensure that the chosen CHIP and CORE are supported. Refer to the documentation for the list of supported chipsets and cores.

==============================================================================
                         INDIVIDUAL BUILD EXAMPLES
==============================================================================

For Linux:

   Navigate to the codec directory:
   $ cd VPU_C7

   Create a build directory and navigate into it:
   $ mkdir -p tmp
   $ cd tmp

   Configure the build with CMake:
   $ cmake \
     -DCHIP=TCC805x -DCORE=A72 \
     -DCMAKE_TOOLCHAIN_FILE=../../config/toolchain.arm.linux.config \
     -DOS=Linux \
     -DCMAKE_BUILD_TYPE=Release \
     ..

   Build the project:
   $ make

For Android:

   Navigate to the codec directory:
   $ cd VPU_C7

   Create a build directory and navigate into it:
   $ mkdir -p tmp
   $ cd tmp

   Set up the Android NDK environment:
   $ ENV_NDK_VER=r21
   $ source ../../config/toolchain.arm.android.config

   Configure the build with CMake:
   $ cmake \
     -DCHIP=TCC805x -DCORE=A72 \
     -DCMAKE_TOOLCHAIN_FILE=${ENV_ANDROID_TOOLCHAIN_FILE} \
     -DOS=Android \
     -DCMAKE_BUILD_TYPE=Release \
     -DANDROID_ABI=${ENV_ANDROID_ABI} \
     -DANDROID_PLATFORM=${ENV_ANDROID_PLATFORM} \
     -DANDROID_NDK=${ENV_ANDROID_NDK} \
     ..

   Build the project:
   $ make
