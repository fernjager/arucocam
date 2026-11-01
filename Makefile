# SPDX-License-Identifier: MIT

# ============================================================================
#  arucocam -- ArUco marker detection
#
#  Everything except Docker runs in the build container.  No host OpenCV or
#  cross compiler is required.
#
#    make pi32          build ARMv6 for Pi Zero / Pi 1 (default)
#    make pi64          build AArch64 for Pi 3/4/5, Zero 2 W, CM4/CM5
#    make amd64         build Linux x86-64
#    make check32       ELF checks, synthetic frame, preset/dictionary tests
#    make check64       same checks for AArch64
#    make check-amd64   same checks for x86-64
#    make image         build the Docker build environment
#    make shell         shell inside the build environment
#    make clean         remove build products
# ============================================================================

SHELL := /bin/bash
.DEFAULT_GOAL := pi32

# The pinned musl.cc toolchains are x86_64-host binaries.  Build in an amd64
# container even on Apple silicon; Docker will emulate it.  Override this only
# if you switch to a toolchain built for a different host architecture.
BASE_PLATFORM ?= linux/amd64

TARGET ?= armv6
ifeq ($(TARGET),arm64)
TARGET_TRIPLE := aarch64-linux-musl
TOOLCHAIN_URL := https://musl.cc/aarch64-linux-musl-cross.tgz
TOOLCHAIN_SHA256 := c909817856d6ceda86aa510894fa3527eac7989f0ef6e87b5721c58737a06c38
OPENCV_CPU_BASELINE := NEON
OPENCV_ENABLE_NEON := ON
TOOLCHAIN_FILE := build/cmake/toolchain/aarch64-musl.cmake
BUILD_DIR ?= build/out/arm64
BINARY_NAME ?= arucocam-arm64
else ifeq ($(TARGET),armv6)
TARGET_TRIPLE := armv6-linux-musleabihf
TOOLCHAIN_URL := https://musl.cc/armv6-linux-musleabihf-cross.tgz
TOOLCHAIN_SHA256 := 6521766f7dcfa897cce3612e638c89e357229f41e9c1a1e23899ab6e87a1d8c6
OPENCV_CPU_BASELINE := VFPV2
OPENCV_ENABLE_NEON := OFF
TOOLCHAIN_FILE := build/cmake/toolchain/armv6-musl.cmake
BUILD_DIR ?= build/out/armv6
BINARY_NAME ?= arucocam-arm32
else ifeq ($(TARGET),amd64)
TARGET_TRIPLE := x86_64-linux-musl
TOOLCHAIN_URL := https://musl.cc/x86_64-linux-musl-cross.tgz
TOOLCHAIN_SHA256 := c5d410d9f82a4f24c549fe5d24f988f85b2679b452413a9f7e5f7b956f2fe7ea
OPENCV_CPU_BASELINE := SSE2
OPENCV_ENABLE_NEON := OFF
TOOLCHAIN_FILE := build/cmake/toolchain/x86_64-musl.cmake
BUILD_DIR ?= build/out/amd64
BINARY_NAME ?= arucocam-amd64
else
$(error unsupported TARGET '$(TARGET)'; use armv6, arm64, or amd64)
endif

IMAGE ?= arucocam-build:$(TARGET)
# Bump OPENCV_SHA256 whenever OPENCV_VERSION changes.
OPENCV_VERSION ?= 4.14.0
OPENCV_SHA256 ?= ee8fb9b30eb60850431b4656447080e3737b56e45719c92b67f245950609f86e
# JOBS controls OpenCV build parallelism.  Six is safe in an 8 GB Docker VM.
# It is also a Docker build arg, so `make image` and the binary targets share
# the same cached image.
JOBS ?= 6
DIST_DIR ?= dist

DOCKER_RUN := docker run --rm --platform $(BASE_PLATFORM) \
	-v "$(CURDIR):/project" -w /project \
	-e TARGET=$(TARGET) \
	-e TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
	-e BUILD_DIR=$(BUILD_DIR) \
	-e DIST_DIR=$(DIST_DIR) \
	-e BINARY_NAME=$(BINARY_NAME) \
	-e JOBS=$(JOBS)

.PHONY: help image pi pi32 pi64 amd64 binary check check32 check64 check-amd64 run-check tidy shell clean distclean inspect

help:
	@echo "arucocam -- ArUco marker detection"
	@echo ""
	@echo "  make image          Build the Docker build environment for TARGET"
	@echo "  make pi32           Cross-compile for ARMv6 (Pi Zero / Pi 1)"
	@echo "  make pi64           Cross-compile for AArch64 (Pi 3/4/5, Zero 2 W)"
	@echo "  make amd64          Cross-compile for Linux x86-64"
	@echo "  make check32        Build tests + run ARMv6 checks"
	@echo "  make check64        Build tests + run AArch64 checks"
	@echo "  make check-amd64    Build tests + run x86-64 checks"
	@echo "  make tidy           Run clang-tidy in the build environment"
	@echo "  make shell          Open a shell in the build environment"
	@echo "  make inspect        Verify the built binary"
	@echo "  make clean          Remove build/out/ and dist/"
	@echo "  make distclean      clean + remove the Docker image"
	@echo ""
	@echo "  TARGET=$(TARGET) BASE_PLATFORM=$(BASE_PLATFORM) OPENCV_VERSION=$(OPENCV_VERSION) JOBS=$(JOBS)"
	@echo "  output: $(DIST_DIR)/$(BINARY_NAME)"

image:
	docker buildx build --load \
		--platform $(BASE_PLATFORM) \
		-t $(IMAGE) \
		--build-arg BASE_PLATFORM=$(BASE_PLATFORM) \
		--build-arg TARGET=$(TARGET) \
		--build-arg TARGET_TRIPLE=$(TARGET_TRIPLE) \
		--build-arg TOOLCHAIN_URL=$(TOOLCHAIN_URL) \
		--build-arg TOOLCHAIN_SHA256=$(TOOLCHAIN_SHA256) \
		--build-arg OPENCV_CPU_BASELINE=$(OPENCV_CPU_BASELINE) \
		--build-arg OPENCV_ENABLE_NEON=$(OPENCV_ENABLE_NEON) \
		--build-arg OPENCV_VERSION=$(OPENCV_VERSION) \
		--build-arg OPENCV_SHA256=$(OPENCV_SHA256) \
		--build-arg JOBS=$(JOBS) \
		-f build/docker/Dockerfile .

# Compatibility aliases; prefer pi32/pi64 and check32/check64.
pi:
	$(MAKE) binary TARGET=$(TARGET)

pi32:
	$(MAKE) binary TARGET=armv6

pi64:
	$(MAKE) binary TARGET=arm64

amd64:
	$(MAKE) binary TARGET=amd64

binary: image
	$(DOCKER_RUN) $(IMAGE) ./build/scripts/build.sh

check:
	$(MAKE) run-check TARGET=$(TARGET)

check32:
	$(MAKE) run-check TARGET=armv6

check64:
	$(MAKE) run-check TARGET=arm64

check-amd64:
	$(MAKE) run-check TARGET=amd64

run-check: image
	$(DOCKER_RUN) -e ARUCOCAM_BUILD_TESTS=ON $(IMAGE) \
		bash -c 'ARUCOCAM_BUILD_TESTS=ON ./build/scripts/build.sh && ./build/scripts/check.sh'

tidy: image
	$(DOCKER_RUN) $(IMAGE) ./build/scripts/tidy.sh

shell: image
	docker run --rm -it --platform $(BASE_PLATFORM) \
		-v "$(CURDIR):/project" -w /project $(IMAGE) /bin/bash

inspect:
	@if [ -f "$(DIST_DIR)/$(BINARY_NAME)" ]; then \
		$(DOCKER_RUN) $(IMAGE) \
			bash -c 'TARGET=$(TARGET) ./build/scripts/check-binary.sh "$(DIST_DIR)/$(BINARY_NAME)"'; \
	else \
		echo "$(DIST_DIR)/$(BINARY_NAME) not built"; \
	fi

clean:
	@test -n "$(BUILD_DIR)" || { echo "BUILD_DIR is empty; refusing to remove"; exit 1; }
	rm -rf -- "$(CURDIR)/build/out" "$(CURDIR)"/build/tidy-* \
		"$(CURDIR)/dist" "$(CURDIR)/dist-armv6" "$(CURDIR)/dist-arm64" \
		"$(CURDIR)/dist-amd64" \
		"$(CURDIR)/tests/marker.pgm" "$(CURDIR)/tests/marker.jpg" \
		"$(CURDIR)/tests/marker.i420"

distclean: clean
	-docker image rm -f arucocam-build:armv6 arucocam-build:arm64 \
		arucocam-build:amd64
