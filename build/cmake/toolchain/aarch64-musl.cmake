# ============================================================================
#  aarch64-musl.cmake -- Raspberry Pi 3/4/5, Zero 2 W, CM4/CM5 (64-bit)
#
#  Target: ARMv8-A, AArch64, little endian, musl.
#  Bootlin's aarch64 Buildroot 2026.08 toolchain provides the compiler and
#  sysroot; OpenCV is cross-compiled with these settings.
#
#  Output is a single statically linked AArch64 binary.
# ============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(AARCH64_MUSL_TOOLS_PATH "/opt/toolchain"
    CACHE PATH "Root of the Bootlin AArch64 musl toolchain")
set(AARCH64_MUSL_PREFIX "aarch64-linux"
    CACHE STRING "Compiler wrapper prefix for the AArch64 toolchain")
set(AARCH64_MUSL_SYSROOT
    "/opt/toolchain/aarch64-buildroot-linux-musl/sysroot"
    CACHE PATH "Bootlin AArch64 musl sysroot")

set(AARCH64_MUSL_ARCH_FLAGS "-march=armv8-a"
    CACHE STRING "Architecture flags for the Raspberry Pi ARMv8-A target")

set(CMAKE_SYSROOT "${AARCH64_MUSL_SYSROOT}")

set(CMAKE_C_COMPILER   "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_PREFIX}-g++")
set(CMAKE_ASM_COMPILER "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_PREFIX}-gcc")
set(CMAKE_AR           "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_PREFIX}-ar")
set(CMAKE_RANLIB       "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_PREFIX}-ranlib")
set(CMAKE_STRIP        "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_PREFIX}-strip")
set(CMAKE_OBJCOPY      "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_PREFIX}-objcopy")
set(CMAKE_OBJDUMP      "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_PREFIX}-objdump")

set(CMAKE_C_FLAGS_INIT   "${AARCH64_MUSL_ARCH_FLAGS} -fno-pie")
set(CMAKE_CXX_FLAGS_INIT "${AARCH64_MUSL_ARCH_FLAGS} -fno-pie")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -no-pie")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_C_FLAGS_RELEASE   "-Os -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "" FORCE)

# AArch64 has native 64-bit atomics; libatomic is still present in the
# toolchain and linking it keeps the link line identical across targets.
set(CMAKE_C_STANDARD_LIBRARIES "-latomic")
set(CMAKE_CXX_STANDARD_LIBRARIES "-latomic")

set(CMAKE_FIND_ROOT_PATH "${AARCH64_MUSL_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
