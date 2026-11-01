# Third-Party Notices

`arucocam` itself is licensed under the MIT License; see [LICENSE](LICENSE).

The pre-built binaries published in GitHub Releases are statically linked and
therefore include the third-party components listed below.  Each component
remains under its own license.  The full license texts are in
[`LICENSES/`](LICENSES/).

This file reflects the build configuration in
[`build/docker/Dockerfile`](build/docker/Dockerfile): OpenCV 4.14.0 with the
`core`, `imgproc`, `imgcodecs`, `calib3d`, `objdetect`, `features2d`, and
`flann` modules.  If those build options change, regenerate the notices.

## Bundled components

| Component | Version | License | License text |
| --- | --- | --- | --- |
| OpenCV | 4.14.0 | Apache-2.0 | `LICENSES/OpenCV-LICENSE.txt`, `LICENSES/OpenCV-COPYRIGHT.txt` |
| libjpeg-turbo | bundled with OpenCV | BSD-3-Clause, IJG, zlib | `LICENSES/libjpeg-turbo-LICENSE.md`, `LICENSES/libjpeg-turbo-README.ijg` |
| libpng | bundled with OpenCV | libpng-2.0 | `LICENSES/libpng-LICENSE`, `LICENSES/libpng-README` |
| zlib | bundled with OpenCV | zlib | `LICENSES/zlib-LICENSE` |
| zlib-ng | bundled with OpenCV | zlib | `LICENSES/zlib-ng-LICENSE.md` |
| cpufeatures | bundled with OpenCV | Apache-2.0 | `LICENSES/cpufeatures-LICENSE.txt` |
| quirc | bundled with OpenCV | ISC | `LICENSES/quirc-LICENSE.txt` |
| Berkeley SoftFloat | bundled with OpenCV | BSD-3-Clause | `LICENSES/SoftFloat-COPYING.txt` |
| DLPack | bundled with OpenCV | Apache-2.0 | `LICENSES/dlpack-LICENSE.txt` |
| FlatBuffers | bundled with OpenCV | Apache-2.0 | `LICENSES/flatbuffers-LICENSE.txt` |
| MSCR chi_table | bundled with OpenCV | BSD-3-Clause | `LICENSES/mscr-chi_table-LICENSE.txt` |
| musl libc | 1.2.6 | MIT | `LICENSES/musl-COPYRIGHT.txt` |
| GCC runtime libraries (`libstdc++`, `libgcc`, `libatomic`) | 15.3.0 | GPL-3.0-or-later WITH GCC-Runtime-Library-Exception-3.1 | `LICENSES/GPL-3.0.txt`, `LICENSES/GCC-Runtime-Library-Exception-3.1.txt` |

## Notes

- OpenCV is distributed under the Apache License 2.0.  Its bundled third-party
  components are listed in the table above; the authoritative copies of their
  notices ship in OpenCV's `share/licenses/opencv4` directory.
- `libjpeg-turbo` is covered by the BSD-3-Clause license, the Independent JPEG
  Group (IJG) license, and the zlib license.  All three are included in its
  upstream license file and READMEs.
- `musl` is linked into the static binary as the C library.  Its full
  copyright notice and MIT license are in `LICENSES/musl-COPYRIGHT.txt`.
- The GCC runtime libraries are linked into the static binary under GPLv3 with
  the GCC Runtime Library Exception 3.1.  The exception permits this linking
  without making the rest of the binary subject to the GPL.
- The compiler toolchains are pinned prebuilt Bootlin Buildroot 2026.08
  tarballs (GCC 15.3.0, binutils 2.45.1); only their runtime libraries are
  linked into the binaries.  The toolchain SHA-256 pins are in the Dockerfile
  and CI workflow.
- Build-time tools and base images (Debian, CMake, Ninja, GCC, binutils,
  musl-cross-make, Docker) are not distributed as part of the released binary
  and are therefore not listed above.

This notice is provided for convenience and is not legal advice.  If you
redistribute a binary built with a different configuration, review the
corresponding upstream license files and update this notice.
