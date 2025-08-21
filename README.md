### Telechips VPU Core

## Overview
Telechips VPU Core provides the host-side driver control code and the capability to load IP firmware onto Telechips video IP. The project is designed to be cross-platform and can be built for different operating systems by selecting the proper toolchain and configuration. TOPST currently supports Linux, and default build scripts are prepared for cross-compiling to aarch64/arm targets.

## Supported Codecs
- **Decoding**: AVC (H.264), HEVC (H.265), VP9
- **Encoding**: AVC (H.264), HEVC (H.265)

## Prerequisites
- **GCC**: 9.2 or newer (cross compilers configured in `config/`)
- **CMake**: Version compatible with your environment (use your standard distro/NDK CMake)
- **Android NDK (if targeting Android)**: Use a recommended NDK version and set up via `config/toolchain.arm64.android.config`
  - Android 10 (API 29): r21
  - Android 11 (API 30): r21, r22, r23
  - Android 12/12L (API 31/32): r23, r25b, r25c
  - Android 13 (API 33): r25b, r25c

## Repository Layout
```
00_ReadMeFirst.txt                 – Project overview and usage notes
01_ReadMe_For_Linux_or_Android.txt – Build guide for Linux/Android
build.sh                           – Build helper script
CMakeLists.txt                     – Top-level CMake configuration
bin/                               – Build outputs per codec
codec_common/                      – Common sources shared by codecs
config/                            – Build configs and toolchains
  common_config.cmake              – Common build logic (requires CHIP/CORE)
  common_version.cmake             – Version settings
  TCC75xx.cmake, TCC805x.cmake...  – Chip-specific configs
  tcc_chipset_info.txt             – Chipset list (used by scripts)
  tcc_video_ip_info.txt            – Video IP list (used by scripts)
  toolchain.arm*.config            – Linux/Android toolchain settings (arm/arm64)
${CODEC_NAME}/                     – Per-IP codec directory (e.g., VPU4K_D2, VPU_C7)
  ${CODEC_NAME}_ko.c               – Kernel interface for the codec
  source/lib_vpu_codec/            – Codec library sources
    api/                           – Public APIs
    tcc_vpu/bitcode/               – IP firmware bitcode
    vpuapi/                        – IP-related sources
```

## Configuration Keys
- **CHIP**: Target chip selection (e.g., `TCC805x`). Mandatory.
- **CORE**: Target CPU core (e.g., `A72`). Mandatory.
- These are validated by `config/common_config.cmake` and must be passed to CMake using `-DCHIP=... -DCORE=...`.

## Toolchains
- Linux aarch64 cross toolchain: `config/toolchain.arm64.linux.config`
  - Uses GCC 9.2 toolchain prefix `aarch64-none-linux-gnu-`
  - Default locations: `/opt` or `/home/$USER/opt`
- Android arm64 setup script: `config/toolchain.arm64.android.config`
  - Export `ENV_NDK_VER` (e.g., `r21`, `r23c`, `r25b`, `r25c`), then `source` the script
  - Provides `ENV_ANDROID_ABI`, `ENV_ANDROID_PLATFORM`, `ENV_ANDROID_NDK`, and `ENV_ANDROID_TOOLCHAIN_FILE`
- 32-bit variants for Linux/Android are also available under `config/` if needed.

## Build Outputs
- Build artifacts are collected under `bin/` per codec (e.g., `bin/VPU4K_D2`).
- Use these outputs to build the target OS driver or integrate with your OS-specific build system.

### Using with TOPST
- For TOPST, place the build outputs inside `topst-vpu` under the directory named after the corresponding IP, then run the TOPST build.
- Refer to `topst-vpu` documentation for exact directory layout and integration steps.

## How to Build
There are two ways to build: using the helper script or per-codec manual builds.

### Option 1) Scripted Build
Use `./build.sh` to automate chipset selection and build. Ensure toolchains and environment variables are properly configured before running.

### Option 2) Per-Codec Manual Build

#### Linux (example with VPU_C7)
```bash
cd VPU_C7
mkdir -p tmp && cd tmp
cmake \
  -DCHIP=TCC805x -DCORE=A72 \
  -DCMAKE_TOOLCHAIN_FILE=../../config/toolchain.arm.linux.config \
  -DOS=Linux \
  -DCMAKE_BUILD_TYPE=Release \
  ..
make
```

#### Android (example with VPU_C7)
```bash
cd VPU_C7
mkdir -p tmp && cd tmp
export ENV_NDK_VER=r21
source ../../config/toolchain.arm.android.config
cmake \
  -DCHIP=TCC805x -DCORE=A72 \
  -DCMAKE_TOOLCHAIN_FILE=${ENV_ANDROID_TOOLCHAIN_FILE} \
  -DOS=Android \
  -DCMAKE_BUILD_TYPE=Release \
  -DANDROID_ABI=${ENV_ANDROID_ABI} \
  -DANDROID_PLATFORM=${ENV_ANDROID_PLATFORM} \
  -DANDROID_NDK=${ENV_ANDROID_NDK} \
  ..
make
```

## Notes & Tips
- Always verify that the selected `CHIP` and `CORE` are supported by your chosen codec/IP.
- On Linux builds, outputs will be placed in `bin/`. Use them to produce a kernel object (KO) if required by your target OS; additional KO build steps must be performed in a proper kernel build environment.
- If you modify shared files, rebuild all codecs and ensure there are no errors.

## License & Support
This repository is intended for partners/customers of Telechips with appropriate access rights. For licensing or technical support, please contact Telechips through your usual support channel.


