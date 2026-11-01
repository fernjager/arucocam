#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Run clang-tidy against the project sources inside the build image.
set -euo pipefail

cd "$(dirname "$0")/../.."

TARGET=${TARGET:-amd64}
BUILD_DIR=${BUILD_DIR:-build/tidy-${TARGET}}
OpenCV_DIR=${OpenCV_DIR:-/opt/opencv/lib/cmake/opencv4}

case "${TARGET}" in
    armv6)
        TRIPLE=armv6-linux-musleabihf
        TOOLCHAIN=build/cmake/toolchain/armv6-musl.cmake
        ;;
    arm64)
        TRIPLE=aarch64-linux-musl
        TOOLCHAIN=build/cmake/toolchain/aarch64-musl.cmake
        ;;
    amd64)
        TRIPLE=x86_64-linux-musl
        TOOLCHAIN=build/cmake/toolchain/x86_64-musl.cmake
        ;;
    *)
        echo "tidy: unknown TARGET '${TARGET}' (use armv6, arm64, or amd64)" >&2
        exit 1
        ;;
esac

CXX="${TRIPLE}-g++"
SYSROOT=$("${CXX}" -print-sysroot)
GCC_VER=$("${CXX}" -dumpfullversion)
CXX_INCLUDE="${SYSROOT}/include/c++/${GCC_VER}"
CXX_TARGET_INCLUDE="${CXX_INCLUDE}/${TRIPLE}"
RESOURCE_DIR=$(find /usr/lib/llvm-* -path '*/lib/clang/*/include' -type d 2>/dev/null | sort -V | tail -1)

if [[ -z "${RESOURCE_DIR}" ]]; then
    echo "tidy: clang resource headers not found" >&2
    exit 1
fi

cmake -S . -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${PWD}/${TOOLCHAIN}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOpenCV_DIR="${OpenCV_DIR}" \
    -DARUCOCAM_STATIC=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

clang-tidy -p "${BUILD_DIR}" \
    --extra-arg-before=-nostdinc \
    --extra-arg-before="-isystem${RESOURCE_DIR}" \
    --extra-arg-before="-isystem${CXX_INCLUDE}" \
    --extra-arg-before="-isystem${CXX_TARGET_INCLUDE}" \
    --extra-arg-before="-isystem${SYSROOT}/include" \
    src/*.cpp
