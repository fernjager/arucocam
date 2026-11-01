# syntax=docker/dockerfile:1
# SPDX-License-Identifier: MIT
#
# arucocam build environment
# ========================
#
# Cross-compiles a statically linked ArUco marker detection utility for:
#
#   TARGET=armv6  Raspberry Pi Zero / Pi 1 (ARM1176JZF-S, ARMv6Z, VFPv2)
#   TARGET=arm64  Raspberry Pi 3/4/5, Zero 2 W, CM4/CM5 (ARMv8-A, AArch64)
#   TARGET=amd64  Linux x86-64 (generic x86-64, SSE2 baseline)
#
# The ARMv6 binary also runs on later 32-bit Raspberry Pi OS systems.  The
# toolchains are pinned prebuilt musl-cross-make tarballs from musl.cc; they
# are x86_64-host binaries and are not rebuilt from source here.  Build with
# BASE_PLATFORM=linux/amd64 even on Apple silicon (Docker will emulate it).
# That turns a ~30-minute GCC bootstrap into a ~100 MB download.  The SHA-256
# pins are part of the supply chain: update them deliberately when changing a
# toolchain.
#
# Build arguments:
#   BASE_PLATFORM          linux/amd64 (the prebuilt toolchains are x86_64)
#   TARGET                 armv6, arm64, or amd64
#   TARGET_TRIPLE          armv6-linux-musleabihf, aarch64-linux-musl, or
#                          x86_64-linux-musl
#   TOOLCHAIN_URL          pinned prebuilt toolchain tarball
#   TOOLCHAIN_SHA256       SHA-256 of TOOLCHAIN_URL
#   OPENCV_CPU_BASELINE    VFPV2 (armv6), NEON (arm64), or SSE2 (amd64)
#   OPENCV_ENABLE_NEON     OFF (armv6/amd64) or ON (arm64)
#   OPENCV_VERSION         OpenCV release tag
#   OPENCV_SHA256          SHA-256 of the OpenCV source tarball
#   JOBS                   build parallelism

ARG BASE_PLATFORM=linux/amd64
ARG TARGET=armv6
ARG TARGET_TRIPLE=armv6-linux-musleabihf
ARG TOOLCHAIN_URL=https://musl.cc/armv6-linux-musleabihf-cross.tgz
ARG TOOLCHAIN_SHA256=6521766f7dcfa897cce3612e638c89e357229f41e9c1a1e23899ab6e87a1d8c6
ARG OPENCV_CPU_BASELINE=VFPV2
ARG OPENCV_ENABLE_NEON=OFF
ARG OPENCV_VERSION=4.14.0
ARG OPENCV_SHA256=ee8fb9b30eb60850431b4656447080e3737b56e45719c92b67f245950609f86e
ARG JOBS=6

# ---------------------------------------------------------------------------
# Stage 1: prebuilt musl toolchain
# ---------------------------------------------------------------------------
# Pinned multi-arch debian:trixie-slim manifest digest (2026-10-31).
# Bump this deliberately when refreshing the base image.
FROM --platform=${BASE_PLATFORM} debian:trixie-slim@sha256:f6b08b09a05725f9431502d3b5714f5c4576e82c1a84fa1debe0c24a7e0c3ec4 AS toolchain

ARG TARGET
ARG TARGET_TRIPLE
ARG TOOLCHAIN_URL
ARG TOOLCHAIN_SHA256

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
 && rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    curl -fsSL "${TOOLCHAIN_URL}" -o /tmp/toolchain.tgz; \
    echo "${TOOLCHAIN_SHA256}  /tmp/toolchain.tgz" | sha256sum -c -; \
    mkdir -p /opt; \
    tar -xzf /tmp/toolchain.tgz -C /opt; \
    mv "/opt/${TARGET_TRIPLE}-cross" /opt/toolchain; \
    rm -f /tmp/toolchain.tgz; \
    test -x "/opt/toolchain/bin/${TARGET_TRIPLE}-gcc"

# Prove the toolchain targets the requested CPU family before the OpenCV
# build.  The prebuilt ARMv6 toolchain defaults to ARMv5, so the project's
# explicit ARMv6/VFPv2 flags are essential here.
RUN set -eux; \
    TC="/opt/toolchain/bin/${TARGET_TRIPLE}"; \
    case "${TARGET}" in \
      armv6) ARCH_FLAGS="-march=armv6 -mfpu=vfp -mfloat-abi=hard" ;; \
      arm64) ARCH_FLAGS="-march=armv8-a" ;; \
      amd64) ARCH_FLAGS="-march=x86-64" ;; \
      *) echo "unsupported TARGET ${TARGET}"; exit 1;; \
    esac; \
    echo 'int main(void){return 0;}' > /tmp/t.c; \
    "${TC}-gcc" -static -no-pie ${ARCH_FLAGS} /tmp/t.c -o /tmp/t; \
    case "${TARGET}" in \
      armv6) \
        "${TC}-readelf" -A /tmp/t | grep -i 'Tag_CPU_arch: v6'; \
        "${TC}-readelf" -A /tmp/t | grep -i 'Tag_FP_arch: VFPv2'; \
        "${TC}-readelf" -A /tmp/t | grep -i 'Tag_ABI_VFP_args: VFP registers'; \
        "${TC}-gcc" ${ARCH_FLAGS} -Q --help=target /tmp/t.c 2>/dev/null \
            | grep -E -- '-march=[[:space:]]+armv6([+]fp)?([[:space:]]|$)'; \
        "${TC}-gcc" ${ARCH_FLAGS} -Q --help=target /tmp/t.c 2>/dev/null \
            | grep -E -- '-mfpu=[[:space:]]+vfp([[:space:]]|$)'; \
        ;; \
      arm64) \
        "${TC}-readelf" -h /tmp/t | grep -i 'AArch64'; \
        "${TC}-readelf" -A /tmp/t 2>/dev/null | grep -i 'Tag_CPU_arch: v8' || true; \
        ;; \
      amd64) \
        "${TC}-readelf" -h /tmp/t | grep -i 'X86-64'; \
        "${TC}-gcc" ${ARCH_FLAGS} -Q --help=target /tmp/t.c 2>/dev/null \
            | grep -E -- '-march=[[:space:]]+x86-64([[:space:]]|$)'; \
        ;; \
    esac; \
    rm -f /tmp/t.c /tmp/t

# ---------------------------------------------------------------------------
# Stage 2: static OpenCV
# ---------------------------------------------------------------------------
FROM --platform=${BASE_PLATFORM} debian:trixie-slim@sha256:f6b08b09a05725f9431502d3b5714f5c4576e82c1a84fa1debe0c24a7e0c3ec4 AS opencv

ARG TARGET
ARG TARGET_TRIPLE
ARG OPENCV_CPU_BASELINE
ARG OPENCV_ENABLE_NEON
ARG OPENCV_VERSION
ARG OPENCV_SHA256
ARG JOBS=6

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        xz-utils \
        cmake \
        ninja-build \
        pkg-config \
        file \
 && rm -rf /var/lib/apt/lists/*

COPY --from=toolchain /opt/toolchain /opt/toolchain
COPY build/cmake/toolchain /toolchain

RUN set -eux; \
    mkdir -p /build; \
    curl -fsSL "https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VERSION}.tar.gz" \
        -o /tmp/opencv.tar.gz; \
    echo "${OPENCV_SHA256}  /tmp/opencv.tar.gz" | sha256sum -c -; \
    tar -xzf /tmp/opencv.tar.gz -C /build --strip-components=1; \
    rm -f /tmp/opencv.tar.gz

RUN set -eux; \
    case "${TARGET}" in \
      armv6) TOOLCHAIN=armv6-musl.cmake ;; \
      arm64) TOOLCHAIN=aarch64-musl.cmake ;; \
      amd64) TOOLCHAIN=x86_64-musl.cmake ;; \
      *) echo "unsupported TARGET ${TARGET}"; exit 1 ;; \
    esac; \
    cmake -S /build -B /build/build -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="/toolchain/${TOOLCHAIN}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/opencv \
        -DOPENCV_DOWNLOAD_PATH=/build/downloads \
        -DCMAKE_POSITION_INDEPENDENT_CODE=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_LIST=core,imgproc,imgcodecs,calib3d,objdetect,features2d,flann \
        -DBUILD_TESTS=OFF \
        -DBUILD_PERF_TESTS=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_DOCS=OFF \
        -DBUILD_JAVA=OFF \
        -DBUILD_opencv_apps=OFF \
        -DBUILD_opencv_python3=OFF \
        -DBUILD_opencv_js=OFF \
        -DBUILD_opencv_gapi=OFF \
        -DWITH_ADE=OFF \
        -DWITH_ITT=OFF \
        -DBUILD_PROTOBUF=OFF \
        -DWITH_PROTOBUF=OFF \
        -DWITH_IPP=OFF \
        -DWITH_OPENCL=OFF \
        -DWITH_TBB=OFF \
        -DWITH_OPENMP=OFF \
        -DWITH_EIGEN=OFF \
        -DWITH_LAPACK=OFF \
        -DWITH_OPENBLAS=OFF \
        -DWITH_QT=OFF \
        -DWITH_GTK=OFF \
        -DWITH_FFMPEG=OFF \
        -DWITH_GSTREAMER=OFF \
        -DWITH_V4L=OFF \
        -DWITH_1394=OFF \
        -DWITH_OPENEXR=OFF \
        -DWITH_WEBP=OFF \
        -DWITH_TIFF=OFF \
        -DWITH_JASPER=OFF \
        -DWITH_OPENJPEG=OFF \
        -DWITH_GIF=OFF \
        -DWITH_AVIF=OFF \
        -DWITH_JPEG=ON \
        -DBUILD_JPEG=ON \
        -DENABLE_LIBJPEG_TURBO_SIMD=OFF \
        -DWITH_PNG=ON \
        -DBUILD_PNG=ON \
        -DWITH_ZLIB=ON \
        -DBUILD_ZLIB=ON \
        -DWITH_IMGCODEC_PXM=ON \
        -DWITH_IMGCODEC_SUNRASTER=ON \
        -DWITH_IMGCODEC_HDR=OFF \
        -DWITH_IMGCODEC_PFM=OFF \
        -DENABLE_NEON="${OPENCV_ENABLE_NEON}" \
        -DENABLE_VFPV3=OFF \
        -DCPU_BASELINE="${OPENCV_CPU_BASELINE}" \
        -DCPU_DISPATCH= \
        -DOPENCV_ENABLE_NONFREE=OFF \
        -DOPENCV_GENERATE_PKGCONFIG=ON \
        -DOPENCV_SKIP_PYTHON_LOADER=ON; \
    cmake --build /build/build -j"${JOBS}"; \
    cmake --install /build/build; \
    find /opt/opencv/lib -maxdepth 1 -name 'libopencv_*.a' | sort; \
    file /opt/opencv/lib/libopencv_core.a

# Verify the installed archives target the requested architecture.  For ARMv6
# also reject NEON/VFPv3 objects; those fault on the ARM1176.
RUN set -eux; \
    TC="/opt/toolchain/bin/${TARGET_TRIPLE}"; \
    tmp=$(mktemp -d); \
    trap 'rm -rf "$tmp"' EXIT; \
    for a in /opt/opencv/lib/libopencv_*.a; do \
        obj=$("${TC}-ar" t "$a" | head -1); \
        "${TC}-ar" p "$a" "$obj" > "$tmp/one.o"; \
        case "${TARGET}" in \
          armv6) \
            "${TC}-readelf" -A "$tmp/one.o" 2>/dev/null \
                | grep -q 'Tag_CPU_arch: v6' || { echo "not ARMv6: $a"; exit 1; }; \
            "${TC}-readelf" -A "$tmp/one.o" 2>/dev/null \
                | grep -q 'Tag_FP_arch: VFPv2' || { echo "not VFPv2: $a"; exit 1; }; \
            if "${TC}-readelf" -A "$tmp/one.o" 2>/dev/null \
                    | grep -q 'Tag_Advanced_SIMD_arch: NEON'; then \
                echo "NEON object in $a"; exit 1; \
            fi; \
            ;; \
          arm64) \
            "${TC}-readelf" -h "$tmp/one.o" 2>/dev/null \
                | grep -q 'AArch64' || { echo "not AArch64: $a"; exit 1; }; \
            ;; \
          amd64) \
            "${TC}-readelf" -h "$tmp/one.o" 2>/dev/null \
                | grep -q 'X86-64' || { echo "not x86-64: $a"; exit 1; }; \
            ;; \
          *) echo "unsupported TARGET ${TARGET}"; exit 1;; \
        esac; \
    done

# ---------------------------------------------------------------------------
# Stage 3: build/run image
# ---------------------------------------------------------------------------
FROM --platform=${BASE_PLATFORM} debian:trixie-slim@sha256:f6b08b09a05725f9431502d3b5714f5c4576e82c1a84fa1debe0c24a7e0c3ec4 AS dev

ARG TARGET
ARG TARGET_TRIPLE
ARG JOBS=6

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        ninja-build \
        pkg-config \
        file \
        qemu-user \
        python3 \
        clang-tidy \
        less \
        vim-tiny \
 && rm -rf /var/lib/apt/lists/*

COPY --from=toolchain /opt/toolchain /opt/toolchain
COPY --from=opencv /opt/opencv /opt/opencv

ENV PATH=/opt/toolchain/bin:${PATH} \
    OpenCV_DIR=/opt/opencv/lib/cmake/opencv4 \
    ARUCOCAM_TARGET=${TARGET} \
    CMAKE_BUILD_PARALLEL_LEVEL=${JOBS}

WORKDIR /project

RUN set -eux; \
    "/opt/toolchain/bin/${TARGET_TRIPLE}-gcc" --version | head -1; \
    cmake --version | head -1; \
    test -f /opt/opencv/lib/cmake/opencv4/OpenCVConfig.cmake; \
    case "${TARGET}" in \
      armv6) qemu-arm --version | head -1;; \
      arm64) qemu-aarch64 --version | head -1;; \
      amd64) qemu-x86_64 --version | head -1;; \
      *) exit 1;; \
    esac

CMD ["/bin/bash"]
