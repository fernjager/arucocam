# ============================================================================
#  armv6-musl.cmake -- Raspberry Pi Zero (generation 1) cross toolchain
#
#  Target: Broadcom BCM2835 / ARM1176JZF-S, ARMv6Z, VFPv2, hard float,
#          little endian, Alpine Linux (musl).
#
#  The compiler tuple by itself does not pin the CPU: GCC's ARM default is an
#  older ARM core, and Debian's armhf toolchain is ARMv7.  The ARM1176 faults
#  on ARMv7 and NEON instructions, so both the ABI and the instruction set are
#  pinned here and in the prebuilt toolchain flags.
#
#  Used by build/docker/Dockerfile and build/scripts/build.sh.  The executable
#  links statically, so the result is one file with no runtime dependencies.
# ============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(ARMV6_MUSL_TOOLS_PATH "/opt/toolchain"
    CACHE PATH "Root of the armv6-linux-musleabihf cross toolchain")
set(ARMV6_MUSL_TRIPLE "armv6-linux-musleabihf"
    CACHE STRING "GNU target triple of the ARMv6 hard-float musl toolchain")

set(ARMV6_MUSL_ARCH_FLAGS "-march=armv6 -mfpu=vfp -mfloat-abi=hard"
    CACHE STRING "Architecture flags for the BCM2835 / ARM1176JZF-S")

set(CMAKE_C_COMPILER   "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_TRIPLE}-g++")
set(CMAKE_ASM_COMPILER "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_TRIPLE}-gcc")
set(CMAKE_AR           "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_TRIPLE}-ar")
set(CMAKE_RANLIB       "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_TRIPLE}-ranlib")
set(CMAKE_STRIP        "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_TRIPLE}-strip")
set(CMAKE_OBJCOPY      "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_TRIPLE}-objcopy")
set(CMAKE_OBJDUMP      "${ARMV6_MUSL_TOOLS_PATH}/bin/${ARMV6_MUSL_TRIPLE}-objdump")

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

set(CMAKE_FIND_ROOT_PATH "${ARMV6_MUSL_TOOLS_PATH}/${ARMV6_MUSL_TRIPLE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# OpenCV is installed outside the sysroot at /opt/opencv, so package search has
# to look in both places.  Library/header search stays restricted above.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
