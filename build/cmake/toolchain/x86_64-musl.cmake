# ============================================================================
#  x86_64-musl.cmake -- Linux x86-64 (amd64) cross toolchain
#
#  Target: x86-64, little endian, Alpine Linux (musl).
#  The toolchain is built by build/docker/Dockerfile with musl-cross-make using
#  x86_64-linux-musl and --with-arch=x86-64, so the resulting binary runs on
#  both musl and glibc distributions.
#
#  Output is a single statically linked x86-64 ELF binary.
# ============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(X86_64_MUSL_TOOLS_PATH "/opt/toolchain"
    CACHE PATH "Root of the x86_64-linux-musl cross toolchain")
set(X86_64_MUSL_TRIPLE "x86_64-linux-musl"
    CACHE STRING "GNU target triple of the x86-64 musl toolchain")

set(X86_64_MUSL_ARCH_FLAGS "-march=x86-64"
    CACHE STRING "Architecture flags for the generic x86-64 target")

set(CMAKE_C_COMPILER   "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_TRIPLE}-g++")
set(CMAKE_ASM_COMPILER "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_TRIPLE}-gcc")
set(CMAKE_AR           "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_TRIPLE}-ar")
set(CMAKE_RANLIB       "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_TRIPLE}-ranlib")
set(CMAKE_STRIP        "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_TRIPLE}-strip")
set(CMAKE_OBJCOPY      "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_TRIPLE}-objcopy")
set(CMAKE_OBJDUMP      "${X86_64_MUSL_TOOLS_PATH}/bin/${X86_64_MUSL_TRIPLE}-objdump")

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

set(CMAKE_FIND_ROOT_PATH "${X86_64_MUSL_TOOLS_PATH}/${X86_64_MUSL_TRIPLE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# OpenCV is installed outside the sysroot at /opt/opencv, so package search has
# to look in both places.  Library/header search stays restricted above.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
