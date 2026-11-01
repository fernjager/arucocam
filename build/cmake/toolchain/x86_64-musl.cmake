# ============================================================================
#  x86_64-musl.cmake -- Linux x86-64 (amd64) cross toolchain
#
#  Target: x86-64, little endian, musl.
#  Bootlin's x86-64 Buildroot 2026.08 toolchain provides the compiler and
#  sysroot, so the resulting binary runs on both musl and glibc distributions.
#
#  Output is a single statically linked x86-64 ELF binary.
# ============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(X86_64_MUSL_TOOLS_PATH "/opt/toolchain"
    CACHE PATH "Root of the Bootlin x86-64 musl toolchain")
set(X86_64_MUSL_PREFIX "x86_64-linux"
    CACHE STRING "Compiler wrapper prefix for the x86-64 toolchain")
set(X86_64_MUSL_SYSROOT
    "/opt/toolchain/x86_64-buildroot-linux-musl/sysroot"
    CACHE PATH "Bootlin x86-64 musl sysroot")

set(X86_64_MUSL_ARCH_FLAGS "-march=x86-64"
    CACHE STRING "Architecture flags for the generic x86-64 target")

set(CMAKE_SYSROOT "${X86_64_MUSL_SYSROOT}")

set(CMAKE_C_COMPILER   "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_PREFIX}-g++")
set(CMAKE_ASM_COMPILER "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_PREFIX}-gcc")
set(CMAKE_AR           "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_PREFIX}-ar")
set(CMAKE_RANLIB       "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_PREFIX}-ranlib")
set(CMAKE_STRIP        "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_PREFIX}-strip")
set(CMAKE_OBJCOPY      "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_PREFIX}-objcopy")
set(CMAKE_OBJDUMP      "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_PREFIX}-objdump")

set(CMAKE_C_FLAGS_INIT   "${X86_64_MUSL_ARCH_FLAGS} -fno-pie")
set(CMAKE_CXX_FLAGS_INIT "${X86_64_MUSL_ARCH_FLAGS} -fno-pie")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -no-pie")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_C_FLAGS_RELEASE   "-Os -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "" FORCE)

set(CMAKE_C_STANDARD_LIBRARIES "-latomic")
set(CMAKE_CXX_STANDARD_LIBRARIES "-latomic")

set(CMAKE_FIND_ROOT_PATH "${X86_64_MUSL_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# OpenCV is installed outside the sysroot at /opt/opencv, so package search has
# to look in both places.  Library/header search stays restricted above.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
