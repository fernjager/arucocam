# ============================================================================
#  aarch64-musl.cmake -- Raspberry Pi 3/4/5, Zero 2 W, CM4/CM5 (64-bit)
#
#  Target: ARMv8-A, AArch64, little endian, Alpine Linux (musl).
#  The toolchain is built by build/docker/Dockerfile with musl-cross-make using
#  aarch64-linux-musl and --with-arch=armv8-a.
#
#  Output is a single statically linked AArch64 binary.
# ============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(AARCH64_MUSL_TOOLS_PATH "/opt/toolchain"
    CACHE PATH "Root of the aarch64-linux-musl cross toolchain")
set(AARCH64_MUSL_TRIPLE "aarch64-linux-musl"
    CACHE STRING "GNU target triple of the AArch64 musl toolchain")

set(AARCH64_MUSL_ARCH_FLAGS "-march=armv8-a"
    CACHE STRING "Architecture flags for the Raspberry Pi ARMv8-A target")

set(CMAKE_C_COMPILER   "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_TRIPLE}-g++")
set(CMAKE_ASM_COMPILER "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_TRIPLE}-gcc")
set(CMAKE_AR           "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_TRIPLE}-ar")
set(CMAKE_RANLIB       "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_TRIPLE}-ranlib")
set(CMAKE_STRIP        "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_TRIPLE}-strip")
set(CMAKE_OBJCOPY      "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_TRIPLE}-objcopy")
set(CMAKE_OBJDUMP      "${AARCH64_MUSL_TOOLS_PATH}/bin/${AARCH64_MUSL_TRIPLE}-objdump")

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

set(CMAKE_FIND_ROOT_PATH "${AARCH64_MUSL_TOOLS_PATH}/${AARCH64_MUSL_TRIPLE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
