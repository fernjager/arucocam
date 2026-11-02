# Build Provenance

This document records the default inputs for the `arucocam` build.  The
[README](../README.md#build) summarizes the build workflow; this page is the
reference for toolchain versions, checksums, and independent verification.

The Dockerfile downloads each tarball and verifies its SHA-256 before
extracting or compiling it, so a mismatch fails the build rather than silently
using a different toolchain.

## Toolchains

These are the prebuilt Bootlin Buildroot 2026.08 musl toolchains.  The build
uses the GitHub mirror assets so CI does not depend on the Bootlin site; the
upstream Bootlin tarballs are linked alongside them for independent
verification.

| Target | Bootlin upstream tarball | SHA-256 | Build mirror |
| --- | --- | --- | --- |
| `armv6` | [`armv6-eabihf--musl--stable-2026.08-1.tar.xz`](https://toolchains.bootlin.com/downloads/releases/toolchains/armv6-eabihf/tarballs/armv6-eabihf--musl--stable-2026.08-1.tar.xz) | `73521cb071ef27c529c223caf5e8f157d3b87a6d862d20f3a7a01b3e0eb3139e` | [GitHub release asset](https://github.com/fernjager/arucocam/releases/download/toolchains-v2/armv6-eabihf--musl--stable-2026.08-1.tar.xz) |
| `arm64` | [`aarch64--musl--stable-2026.08-1.tar.xz`](https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/aarch64--musl--stable-2026.08-1.tar.xz) | `b388c480a48e8e9f9b99e3d14e69219c4d61e5a2424a82faecb88a015b781a60` | [GitHub release asset](https://github.com/fernjager/arucocam/releases/download/toolchains-v2/aarch64--musl--stable-2026.08-1.tar.xz) |
| `amd64` | [`x86-64--musl--stable-2026.08-1.tar.xz`](https://toolchains.bootlin.com/downloads/releases/toolchains/x86-64/tarballs/x86-64--musl--stable-2026.08-1.tar.xz) | `78d3a4683d6ac47b5ee73bd5bce210b55eb93dff1b137c61298af97eb0d2b5a6` | [GitHub release asset](https://github.com/fernjager/arucocam/releases/download/toolchains-v2/x86-64--musl--stable-2026.08-1.tar.xz) |

Each SHA-256 is the checksum of both the upstream tarball and the mirror; the
mirror is a copy of the upstream file.  The Dockerfile verifies the mirror
against this value at build time.

## Shared Inputs

| Input | Pin |
| --- | --- |
| OpenCV | [`4.14.0` source tarball](https://github.com/opencv/opencv/archive/refs/tags/4.14.0.tar.gz) |
| OpenCV SHA-256 | `ee8fb9b30eb60850431b4656447080e3737b56e45719c92b67f245950609f86e` |
| Build base | `debian:trixie-slim@sha256:f6b08b09a05725f9431502d3b5714f5c4576e82c1a84fa1debe0c24a7e0c3ec4` |
| Build definition | [`build/docker/Dockerfile`](../build/docker/Dockerfile) |

To verify the upstream Bootlin tarball yourself:

```bash
URL=https://toolchains.bootlin.com/downloads/releases/toolchains/armv6-eabihf/tarballs/armv6-eabihf--musl--stable-2026.08-1.tar.xz
SHA=73521cb071ef27c529c223caf5e8f157d3b87a6d862d20f3a7a01b3e0eb3139e
curl -fsSLO "$URL"
echo "$SHA  ${URL##*/}" | sha256sum -c -
```

These SHA-256 values have been checked against the upstream Bootlin tarballs.
The Dockerfile checks the same values against the GitHub mirror used by the
build; to independently prove the mirror is unmodified, download both URLs and
compare their `sha256sum` output.

The [`Makefile`](../Makefile) exposes `TOOLCHAIN_URL`, `TOOLCHAIN_SHA256`,
`OPENCV_VERSION`, and `OPENCV_SHA256`; overriding them changes what is built.
The values above are the defaults used by `make pi32`, `make pi64`, and
`make amd64`.  The toolchains are mirrored under the
[`toolchains-v2` release](https://github.com/fernjager/arucocam/releases/tag/toolchains-v2);
the Dockerfile also pins the `debian:trixie-slim` base image by digest.
