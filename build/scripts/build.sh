#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Cross-compile the arucocam binary inside the build image.
#
# Environment:
#   TARGET          armv6 (default), arm64, or amd64
#   TOOLCHAIN_FILE  CMake toolchain file; default build/cmake/toolchain/...
#   BUILD_DIR       build directory; default build/out/${TARGET}
#   DIST_DIR        output directory; default dist
#   BINARY_NAME     output name; default arucocam-arm32/arm64/amd64
#   JOBS            parallelism
#   ARUCOCAM_BUILD_TESTS  ON to build tests/gen_marker
set -euo pipefail

cd "$(dirname "$0")/../.."

TARGET=${TARGET:-armv6}
BUILD_DIR=${BUILD_DIR:-build/out/${TARGET}}
DIST_DIR=${DIST_DIR:-dist}
JOBS=${JOBS:-$(nproc)}
OpenCV_DIR=${OpenCV_DIR:-/opt/opencv/lib/cmake/opencv4}
ARUCOCAM_BUILD_TESTS=${ARUCOCAM_BUILD_TESTS:-OFF}

case "${TARGET}" in
    armv6)
        STRIP=armv6-linux-musleabihf-strip
        DEFAULT_BINARY=arucocam-arm32
        DEFAULT_TOOLCHAIN=build/cmake/toolchain/armv6-musl.cmake
        ;;
    arm64)
        STRIP=aarch64-linux-musl-strip
        DEFAULT_BINARY=arucocam-arm64
        DEFAULT_TOOLCHAIN=build/cmake/toolchain/aarch64-musl.cmake
        ;;
    amd64)
        STRIP=x86_64-linux-musl-strip
        DEFAULT_BINARY=arucocam-amd64
        DEFAULT_TOOLCHAIN=build/cmake/toolchain/x86_64-musl.cmake
        ;;
    *) echo "unknown TARGET '${TARGET}' (use armv6, arm64, or amd64)" >&2; exit 1 ;;
esac

BINARY_NAME=${BINARY_NAME:-${DEFAULT_BINARY}}
TOOLCHAIN_FILE=${TOOLCHAIN_FILE:-${DEFAULT_TOOLCHAIN}}

cmake -S . -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${PWD}/${TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOpenCV_DIR="${OpenCV_DIR}" \
    -DARUCOCAM_STATIC=ON \
    -DARUCOCAM_BUILD_TESTS="${ARUCOCAM_BUILD_TESTS}"

cmake --build "${BUILD_DIR}" -j"${JOBS}"

mkdir -p "${DIST_DIR}"
"${STRIP}" -s -o "${DIST_DIR}/${BINARY_NAME}" "${BUILD_DIR}/arucocam"

file "${DIST_DIR}/${BINARY_NAME}"
echo "binary: $(du -h "${DIST_DIR}/${BINARY_NAME}" | cut -f1)"

TARGET="${TARGET}" "${PWD}/build/scripts/check-binary.sh" "${DIST_DIR}/${BINARY_NAME}"
