#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Assert that a built executable targets the requested platform class
# and has no runtime library dependencies.
set -euo pipefail

TARGET=${TARGET:-armv6}

case "${TARGET}" in
    armv6)
        READELF=${READELF:-armv6-linux-musleabihf-readelf}
        QEMU=${QEMU:-qemu-arm}
        QEMU_CPU=${QEMU_CPU:-arm1176}
        DEFAULT_BINARY=arucocam-arm32
        ;;
    arm64)
        READELF=${READELF:-aarch64-linux-musl-readelf}
        QEMU=${QEMU:-qemu-aarch64}
        QEMU_CPU=${QEMU_CPU:-max}
        DEFAULT_BINARY=arucocam-arm64
        ;;
    amd64)
        READELF=${READELF:-x86_64-linux-musl-readelf}
        QEMU=${QEMU:-qemu-x86_64}
        # qemu treats an empty QEMU_CPU as "-cpu ''"; leave it unset.
        unset QEMU_CPU
        DEFAULT_BINARY=arucocam-amd64
        ;;
    *)
        echo "check-binary: unknown TARGET '${TARGET}'" >&2
        exit 1
        ;;
esac

BIN=${1:-dist/${DEFAULT_BINARY}}

if [[ ! -f "${BIN}" ]]; then
    echo "check-binary: ${BIN} does not exist" >&2
    exit 1
fi

if ! file "${BIN}" | grep -q 'ELF'; then
    echo "check-binary: ${BIN} is not an ELF executable" >&2
    file "${BIN}" >&2
    exit 1
fi

if ! file "${BIN}" | grep -q 'statically linked'; then
    echo "check-binary: ${BIN} is not statically linked" >&2
    file "${BIN}" >&2
    exit 1
fi

case "${TARGET}" in
    armv6)
        if ! file "${BIN}" | grep -q 'ELF 32-bit LSB executable, ARM'; then
            echo "check-binary: ${BIN} is not a 32-bit ARM ELF" >&2
            file "${BIN}" >&2
            exit 1
        fi
        attrs=$("${READELF}" -A "${BIN}")
        for needle in \
            'Tag_CPU_arch: v6' \
            'Tag_FP_arch: VFPv2' \
            'Tag_ABI_VFP_args: VFP registers'; do
            if ! grep -q "${needle}" <<<"${attrs}"; then
                echo "check-binary: missing '${needle}'" >&2
                echo "${attrs}" >&2
                exit 1
            fi
        done
        if grep -q 'Tag_Advanced_SIMD_arch: NEON' <<<"${attrs}"; then
            echo "check-binary: NEON instructions are present; ARM1176 will fault" >&2
            exit 1
        fi
        ;;
    arm64)
        if ! file "${BIN}" | grep -q 'ELF 64-bit LSB executable, ARM aarch64'; then
            echo "check-binary: ${BIN} is not an AArch64 ELF" >&2
            file "${BIN}" >&2
            exit 1
        fi
        if ! "${READELF}" -h "${BIN}" | grep -q 'AArch64'; then
            echo "check-binary: ${BIN} is not AArch64" >&2
            exit 1
        fi
        ;;
    amd64)
        if ! file "${BIN}" | grep -q 'ELF 64-bit LSB executable, x86-64'; then
            echo "check-binary: ${BIN} is not an x86-64 ELF" >&2
            file "${BIN}" >&2
            exit 1
        fi
        if ! "${READELF}" -h "${BIN}" | grep -q 'X86-64'; then
            echo "check-binary: ${BIN} is not x86-64" >&2
            exit 1
        fi
        ;;
esac

if "${READELF}" -d "${BIN}" 2>/dev/null | grep -q 'NEEDED'; then
    echo "check-binary: dynamic library dependency found" >&2
    exit 1
fi

if command -v "${QEMU}" >/dev/null 2>&1; then
    if [[ -n "${QEMU_CPU:-}" ]]; then
        "${QEMU}" -cpu "${QEMU_CPU}" "${BIN}" --version
    else
        "${QEMU}" "${BIN}" --version
    fi
fi

echo "check-binary: ok (${TARGET}, static)"
