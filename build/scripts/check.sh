#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Container-side checks: ELF target, a synthetic ArUco frame, and smoke tests
# for every documented preset/dictionary.  Runs under qemu for the target.
set -euo pipefail

cd "$(dirname "$0")/../.."

TARGET=${TARGET:-armv6}
BUILD_DIR=${BUILD_DIR:-build/out/${TARGET}}
DIST_DIR=${DIST_DIR:-dist}

case "${TARGET}" in
    armv6)
        DEFAULT_BINARY=arucocam-arm32
        QEMU=${QEMU:-qemu-arm}
        QEMU_CPU=${QEMU_CPU:-arm1176}
        ;;
    arm64)
        DEFAULT_BINARY=arucocam-arm64
        QEMU=${QEMU:-qemu-aarch64}
        QEMU_CPU=${QEMU_CPU:-max}
        ;;
    amd64)
        DEFAULT_BINARY=arucocam-amd64
        QEMU=${QEMU:-qemu-x86_64}
        # qemu treats an empty QEMU_CPU as "-cpu ''"; leave it unset.
        unset QEMU_CPU
        ;;
    *) echo "check: unknown TARGET '${TARGET}'" >&2; exit 1 ;;
esac

BINARY_NAME=${BINARY_NAME:-${DEFAULT_BINARY}}
BIN=${BIN:-${DIST_DIR}/${BINARY_NAME}}

qemu_run() {
    if [[ -n "${QEMU_CPU:-}" ]]; then
        "${QEMU}" -cpu "${QEMU_CPU}" "$@"
    else
        "${QEMU}" "$@"
    fi
}

blank_pgm() {
    local w=$1 h=$2 out=$3
    { printf 'P5\n%s %s\n255\n' "${w}" "${h}"; \
      dd if=/dev/zero bs=$((w * h)) count=1 status=none; } > "${out}"
}

if [[ -n "${QEMU_CPU:-}" ]]; then
    TARGET="${TARGET}" QEMU="${QEMU}" QEMU_CPU="${QEMU_CPU}" \
        "${PWD}/build/scripts/check-binary.sh" "${BIN}"
else
    TARGET="${TARGET}" QEMU="${QEMU}" \
        "${PWD}/build/scripts/check-binary.sh" "${BIN}"
fi

# No-argument invocation must print help and exit 0.
help_output=$(qemu_run "${BIN}")
if ! grep -q '^Usage:' <<<"${help_output}"; then
    echo "check: no-argument invocation did not print help" >&2
    exit 1
fi
echo "check: no-argument help ok"

# Functional suite: help, JSON schema, pose, marker-size scaling,
# target-id, empty map, detect-only, multi-frame confirmation, CLI errors.
if [[ -x "${BUILD_DIR}/gen_marker" ]]; then
    python3 tests/test_arucocam.py \
        --bin "${BIN}" \
        --gen-marker "${BUILD_DIR}/gen_marker" \
        --qemu "${QEMU}" \
        --qemu-cpu "${QEMU_CPU:-}"
else
    echo "check: gen_marker not built; skipping functional suite" >&2
fi

tmp=$(mktemp -d)
trap 'rm -rf "${tmp}"' EXIT
for res in 640 1280 1920; do
    case "${res}" in
        640) w=640; h=480 ;;
        1280) w=1280; h=720 ;;
        1920) w=1920; h=1080 ;;
    esac
    blank_pgm "${w}" "${h}" "${tmp}/blank-${res}.pgm"
done

for family in v1 v2 cm3 cm3wide aicam imx519 hawkeye owlsight fisheye160; do
    for res in 640 1280 1920; do
        out=$(qemu_run "${BIN}" --preset "${family}-${res}" \
            --image "${tmp}/blank-${res}.pgm" --json)
        if [[ "${out}" != "{}" ]]; then
            echo "check: preset ${family}-${res} -> ${out}" >&2
            exit 1
        fi
    done
done
echo "check: all presets ok"

for dict in \
    DICT_4X4_50 DICT_4X4_100 DICT_4X4_250 DICT_4X4_1000 \
    DICT_5X5_50 DICT_5X5_100 DICT_5X5_250 DICT_5X5_1000 \
    DICT_6X6_50 DICT_6X6_100 DICT_6X6_250 DICT_6X6_1000 \
    DICT_7X7_50 DICT_7X7_100 DICT_7X7_250 DICT_7X7_1000 \
    DICT_ARUCO_ORIGINAL \
    DICT_APRILTAG_16h5 DICT_APRILTAG_25h9 DICT_APRILTAG_36h10 DICT_APRILTAG_36h11; do
    out=$(qemu_run "${BIN}" --preset fisheye160-640 --dict "${dict}" \
        --image "${tmp}/blank-640.pgm" --json)
    if [[ "${out}" != "{}" ]]; then
        echo "check: dictionary ${dict} -> ${out}" >&2
        exit 1
    fi
done
echo "check: all dictionaries ok"

if qemu_run "${BIN}" --preset does-not-exist >/dev/null 2>&1; then
    echo "check: unknown preset unexpectedly succeeded" >&2
    exit 1
fi
echo "check: unknown preset rejected"
