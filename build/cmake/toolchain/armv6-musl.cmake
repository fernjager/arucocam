# ============================================================================
#  armv6-musl.cmake -- Raspberry Pi Zero (generation 1) cross toolchain
#
#  Target: Broadcom BCM2835 / ARM1176JZF-S, ARMv6Z, VFPv2, hard float,
#          little endian, musl.
#
#  Bootlin's armv6-eabihf toolchain is configured for ARM1176 and emits
#  Tag_CPU_arch v6KZ.  The wrapper already selects the right CPU; the explicit
#  -march/-mfpu/-mfloat-abi flags below keep a different toolchain from
#  silently emitting ARMv7 or NEON code.
#
#  Used by build/docker/Dockerfile and build/scripts/build.sh.  The executable
#  links statically, so the result is one file with no runtime dependencies.
# ============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(ARMV6_MUSL_TOOLS_PATH "/opt/toolchain"
    CACHE PATH "Root of the Bootlin ARMv6 musl toolchain")
set(ARMV6_MUSL_PREFIX "arm-linux"
    CACHE STRING "Compiler wrapper prefix for the ARMv6 toolchain")
set(ARMV6_MUSL_SYSROOT
    "/opt/toolchain/arm-buildroot-linux-musleabihf/sysroot"
    CACHE PATH "Bootlin ARMv6 musl sysroot")

set(ARMV6_MUSL_ARCH_FLAGS "-march=armv6 -mfpu=vfp -mfloat-abi=hard"
    CACHE STRING "Architecture flags for the BCM2835 / ARM1176JZF-S")

set(CMAKE_SYSROOT "${ARMV6_MUSL_SYSROOT}")

set(CMAKE_C_COMPILER   "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_PREFIX}-g++")
set(CMAKE_ASM_COMPILER "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_PREFIX}-gcc")
set(CMAKE_AR           "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_PREFIX}-ar")
set(CMAKE_RANLIB       "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_PREFIX}-ranlib")
set(CMAKE_STRIP        "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_PREFIX}-strip")
set(CMAKE_OBJCOPY      "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_PREFIX}-objcopy")
set(CMAKE_OBJDUMP      "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_PREFIX}-objdump")

set(CMAKE_C_FLAGS_INIT   "${ARMV6_MUSL_ARCH_FLAGS} -fno-pie")
set(CMAKE_CXX_FLAGS_INIT "${ARMV6_MUSL_ARCH_FLAGS} -fno-pie")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -no-pie")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()
# -Os matters on a 512 MB, single-core board: the binary and the OpenCV static
# libraries both live in the page cache and the SD card is slow.
set(CMAKE_C_FLAGS_RELEASE   "-Os -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "" FORCE)

# ARMv6 has no LDREXD/STREXD, so 64-bit atomics lower to libatomic calls.
set(CMAKE_C_STANDARD_LIBRARIES "-latomic")
set(CMAKE_CXX_STANDARD_LIBRARIES "-latomic")

set(CMAKE_FIND_ROOT_PATH "${ARMV6_MUSL_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# OpenCV is installed outside the sysroot at /opt/opencv, so package search has
# to look in both places.  Library/header search stays restricted above.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
