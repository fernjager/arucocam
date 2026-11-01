# arucocam

A small, self-contained binary for OpenCV ArUco marker detection.
It prints one compact JSON object per processed image frame, keyed by marker ID:

```json
{"7":{"bearing":2.6,"horiz":1.23,"vert":-0.02}}
```

- `bearing` is degrees left of the camera's forward axis
- `horiz` is planar distance in metres, measured in the camera's horizontal plane
- `vert` is vertical offset in metres, positive up

The origin is the camera's optical centre.  `bearing = 0` is straight ahead,
normally the image centre; `horiz` and `vert` are offsets from that centre, not
image coordinates.  `horiz` is not the straight-line distance; the 3D distance
is `hypot(horiz, vert)`.

A single binary, statically-compiled.
No OpenCV runtime libraries, no config file, no Python on the target.
Calibration is all on the command line.

Supported targets:

| Target | Boards / systems | Binary | Build |
| --- | --- | --- | --- |
| `armv6` | Pi Zero, Pi Zero W, Pi 1 | `dist/arucocam-arm32` | ARM1176JZF-S, VFPv2, **no NEON** |
| `arm64` | Pi 3, Pi 4, Pi 5, Zero 2 W, CM4/CM5 | `dist/arucocam-arm64` | ARMv8-A AArch64 |
| `amd64` | Linux x86-64 | `dist/arucocam-amd64` | generic x86-64, SSE2 baseline |

The ARMv6 binary also runs on later 32-bit Raspberry Pi OS.  A normal armhf
binary may link and then die on the ARM1176; the build pins ARMv6/VFPv2 and
rejects NEON in the final ELF. All three binaries are statically linked
against musl, so the amd64 build runs on glibc and musl distributions without
OpenCV installed.

## Build

Everything runs in Docker; no host OpenCV or cross compiler is needed.  The
image downloads pinned prebuilt musl.cc toolchain tarballs (mirrored as a
GitHub release asset), verifies their SHA-256, and builds OpenCV 4.14.0
statically for each target.  Those toolchains
are x86_64-host binaries, so the build image is `linux/amd64` even on Apple
silicon; Docker emulates it.  The pinned toolchains use GCC 11.2.1 and musl
1.2.2.

```bash
make pi32               # dist/arucocam-arm32    (ARMv6)
make check32            # ELF + functional + preset/dictionary tests

make pi64               # dist/arucocam-arm64    (AArch64)
make check64

make amd64              # dist/arucocam-amd64    (Linux x86-64)
make check-amd64
```

`make pi32` is the default target.  The checks run under QEMU inside the target
image.  The build image is `arucocam-build:<target>`; `make shell` opens it.
The GitHub Actions workflow builds and tests all three targets and uploads
`arucocam-arm32` / `arucocam-arm64` / `arucocam-amd64`; tagged builds also
publish `SHA256SUMS`, a licenses archive, and build provenance.  `v*` tags
create a release.

Deploy:

```bash
scp dist/arucocam-arm32 root@pi:/usr/local/bin/arucocam
# or
scp dist/arucocam-arm64 root@pi:/usr/local/bin/arucocam
# or
scp dist/arucocam-amd64 root@linux:/usr/local/bin/arucocam
```

## Calibration

Use a preset:

```bash
arucocam --preset fisheye160-640 --device /dev/video0 --json
```

Or pass intrinsics directly:

```bash
arucocam --fx 600 --fy 600 --cx 320 --cy 240 \
         --width 640 --height 480 --marker-size 100 --device /dev/video0 --json
```

- `--fx/--fy` are required unless `--preset` is used.
- `--cx/--cy` default to the image centre.
- `--k1..--k4` are fisheye distortion coefficients; default `0`.
- `--preset` cannot be combined with `--fx/--fy/--cx/--cy/--k1..--k4` or
  `--width/--height`.
- `--marker-size` is the **outer black square** in millimetres, not the paper
  or quiet-zone size.  Default `100`.
- Capture and calibrate at the same resolution and aspect ratio.

A preset is a starting point, not a calibration.  For metric distance, calibrate
`fx`, `fy`, `cx`, `cy` and distortion with `cv::fisheye::calibrate` or a
checkerboard tool.

### Presets

Names are `<family>-<resolution>`: `-640` is 640×480, `-1280` is 1280×720,
`-1920` is 1920×1080.  Run `arucocam --list-presets` to print these values and
the horizontal FOV as a table.

| Family | Module / optics | Horizontal FOV | fx=fy at 640 / 1280 / 1920 | 100 mm marker px at 1 m / 3 m |
| --- | --- | ---: | ---: | ---: |
| `v1` | Camera Module V1 / NoIR / Pi Zero camera (OV5647, 3.6 mm) | 53.5° | 635 / 1270 / 1905 | 64 / 21 |
| `v2` | Camera Module V2 / NoIR (IMX219, 3.04 mm) | 62.2° | 530 / 1061 / 1591 | 53 / 18 |
| `cm3` | Camera Module 3 (IMX708, 4.74 mm) | 66° | 493 / 985 / 1478 | 49 / 16 |
| `cm3wide` | Camera Module 3 Wide (IMX708, 2.75 mm) | 102° | 259 / 518 / 777 | 26 / 9 |
| `aicam` | AI Camera (IMX500, 4.74 mm) | 66.3° | 490 / 980 / 1470 | 49 / 16 |
| `imx519` | Arducam 16MP IMX519 autofocus, stock lens | 66° | 493 / 985 / 1478 | 49 / 16 |
| `hawkeye` | Arducam 64MP Hawk-Eye autofocus, stock lens (OV64A40) | 72° | 440 / 881 / 1321 | 44 / 15 |
| `owlsight` | Arducam 64MP OwlSight, 6.65 mm lens (OV64A40) | 68° | 474 / 949 / 1423 | 47 / 16 |
| `fisheye160` | 160° diagonal OV5647 fisheye | 160° diag (≈128° H) | 286 / 573 / 860 | 29 / 10 |

Full names: `v1-640`, `v1-1280`, `v1-1920`, and likewise for the other families.
HFOV is the same at all three resolutions because it depends on the lens.

The Arducam rows assume the stock lens listed; a different lens changes `fx`.
Treat them as starting points, not calibrations.  NoIR variants use the same
preset as the colour version.

The marker-width column uses the 640 px preset and a 100 mm black square.
At 1280 and 1920 the marker is roughly twice and three times as wide, unless
`--process-width` brings processing back to 640.

Notes:

- 160° is the **diagonal** FOV of the fisheye.  The 16:9 presets crop the 4:3
  sensor vertically, so their diagonal FOV is closer to 147°.
- The `cm3-640` and `cm3wide-640` presets are approximate because those sensors
  are 16:9; prefer 1280×720 with `--process-width 640` on a Pi Zero.
- HQ Camera and Global Shutter Camera have C/CS lenses; there is no universal
  preset.  Use `--fx/--fy` or calibrate.

If a preset is far off, calibrate rather than tweaking `fx` by eye.

## Output

Default machine output is one JSON object per processed frame, keyed by marker
ID:

```json
{"7":{"bearing":2.6,"horiz":1.23,"vert":-0.02},"8":{"bearing":-11.4,"horiz":0.82,"vert":0.03}}
```

- Only markers that are **confirmed** and have a valid pose are included.
- No marker: `{}`.
- Multiple markers: one key per marker.
- No timestamp is emitted.  Timestamp on receipt in the controller.
- No target, debug, or full-pose record.

| Field | Unit | Meaning |
| --- | --- | --- |
| `bearing` | degrees | Positive left (CCW), −180 to 180 |
| `horiz` | metres | Planar distance in the camera's horizontal plane |
| `vert` | metres | Vertical offset, positive up, relative to the camera |

Body frame: `x` forward, `y` left, `z` up.  The origin is the camera's optical
centre.  `bearing = 0` is the forward axis, normally the image centre; a
calibrated principal point can be slightly off centre.

```text
x = horiz * cos(bearing)
y = horiz * sin(bearing)
z = vert
```

`vert` is not height above the floor; it is the marker's vertical offset from
the camera.  The true 3D distance is `hypot(horiz, vert)`.

### Precision

Values are printed to 0.1° and 0.01 m.  That is display resolution, not
accuracy.  With a 100 mm marker processed at 640 px, expect roughly 0.1-0.5°
bearing and 1-3% horizontal/vertical error at short distances, 5-10% at longer
distances; fisheye presets are at the worse end.  A 1 mm error in the marker's
black square is a 1% distance bias, and a preset is not a calibration.
`--confirm` gates frames; it does not average.  For better results, calibrate
the lens, use `--corner-refine subpix`, keep the marker large, and tighten
`--max-rms` / `--min-area`.

`--timing` adds a reserved `_timing` key to each JSON line:

```json
{"7":{"bearing":2.6,"horiz":1.23,"vert":-0.02},"_timing":{"processMs":12.345}}
```

`processMs` is the time spent in detection/PnP for that frame, excluding JSON
output and `--max-fps` sleep.  It only affects JSON when explicitly requested.

`--target-id N` restricts output to one ID.  `--confirm N` is the number of
consecutive valid frames before a marker appears; default 3.  A still image
uses `--confirm 1` automatically.

### Text output

Without `--json`, output is human-readable:

```text
id=7 bearing=+2.6deg horiz=1.23m vert=-0.02m
```

Add `--verbose` for RMS and pixel area.  `--detect-only` skips PnP and prints
detected IDs, pixel centres and areas; it is text-only.

### Future binary transport

Reserved for a future version.  Planned little-endian layout:

```text
Header (16 bytes)
  uint32 magic      0x4D414341  ("ACAM")
  uint16 version    1
  uint16 header_len 16
  uint32 marker_len 16
  uint32 count

Marker (16 bytes)
  uint16 id
  int16  bearing_cdeg     degrees * 100, positive left
  uint16 horiz_cm         planar horizontal distance, centimetres
  int16  vert_cm          camera-relative vertical offset, centimetres
  uint16 reserved
  uint32 reserved
```

The JSON protocol is the stable interface for now.

## Running

### V4L2 camera

```bash
arucocam --preset fisheye160-640 --device /dev/video0 --json
arucocam --preset fisheye160-1280 --process-width 640 \
         --device /dev/video0 --max-fps 10 --json
```

The V4L2 path prefers direct-luma formats: `GREY`, `YUV420`, `NV12`, `YUYV`,
`UYVY`, then `MJPEG`/`JPEG`, then `RGB24`/`BGR24`.  YUYV is what the Pi
`bcm2835-v4l2` driver normally offers.  `--max-fps` caps **processing** and
drops stale frames; use `--max-fps 5` when the CPU is pinned.  The program does
not set the camera frame rate or write camera controls: exposure, gain/ISO,
white balance, and frame rate stay whatever `v4l2-ctl` or the driver put in
place.

### stdin

```bash
rpicam-vid -t 0 --codec yuv420 --width 640 --height 480 --framerate 15 --inline -o - |
  arucocam --preset fisheye160-640 --stdin --json
```

`--stdin-format` defaults to `auto`: JPEG input is decoded, anything else is
treated as raw I420.  Use `--stdin-format gray` for raw grayscale, or `mjpeg`
if a producer emits JPEG.  Raw formats require `--width` and `--height`; I420
also requires both to be even.

### Still image

```bash
arucocam --preset fisheye160-640 --image frame.pgm --json
arucocam --preset fisheye160-640 --detect-only --image frame.pgm
```

## Options

| Option | Effect |
| --- | --- |
| `--preset NAME` | Starting intrinsics for a common module. |
| `--list-presets` | Print preset names, intrinsics, and horizontal FOV. |
| `--fx/--fy/--cx/--cy/--k1..--k4` | Manual fisheye calibration. |
| `--width/--height` | Capture/calibration resolution; default 640×480. |
| `--marker-size MM` | Outer black square, millimetres; default 100. |
| `--dict NAME` | ArUco dictionary; default `DICT_4X4_50`. |
| `--process-width N` | Downscale before detection; default 640; `0` = full. |
| `--corner-refine MODE` | `none`, `subpix`, `contour`, `apriltag`. |
| `--max-rms PX` | Reprojection RMS gate; default 5. |
| `--min-area PX2` | Marker area gate; default 50. |
| `--detect-only` | Skip PnP; text centres/areas. |
| `--aruco3` | Enable Aruco3; benchmark first, it was slower at 640×480 in QEMU. |
| `--target-id N` | Only output this marker ID. |
| `--confirm N` | Valid frames before confirmation; default 3. |
| `--stdin-format FMT` | `auto` (default), `mjpeg`, `i420`, or `gray`. |
| `--max-fps N` | Cap processed frames/sec; `0` = unlimited. |
| `--frames N` | Stop after N processed frames. |
| `--json` | Compact map instead of text. |
| `--verbose` | Add RMS/area to text output (not with `--json`). |
| `--timing` | Add `_timing.processMs` to JSON output. |

### Dictionaries

Case-insensitive.  Supported:

`DICT_4X4_50`, `DICT_4X4_100`, `DICT_4X4_250`, `DICT_4X4_1000`,
`DICT_5X5_50`, `DICT_5X5_100`, `DICT_5X5_250`, `DICT_5X5_1000`,
`DICT_6X6_50`, `DICT_6X6_100`, `DICT_6X6_250`, `DICT_6X6_1000`,
`DICT_7X7_50`, `DICT_7X7_100`, `DICT_7X7_250`, `DICT_7X7_1000`,
`DICT_ARUCO_ORIGINAL`, `DICT_APRILTAG_16h5`, `DICT_APRILTAG_25h9`,
`DICT_APRILTAG_36h10`, `DICT_APRILTAG_36h11`.

The sensor and marker must use the same dictionary.

## Troubleshooting

**`{}` every frame.**  No confirmed, valid marker.  Check the dictionary,
marker size, focus, lighting, and that `--preset`/intrinsics match the capture
resolution.  Use `--detect-only` to see whether the marker is found at all.

**Horiz is consistently high or low.**  `--marker-size` is probably wrong, or
the focal length is wrong.  Measure the printed black square, not the paper.

**`image is ... aspect mismatch`.**  Calibrate at the capture resolution or use
a mode with the same aspect ratio.  Cropping changes `cx/cy`.

**A marker appears late.**  It needs `--confirm` consecutive valid poses.  For
a single image this is automatically 1.

**CPU pinned.**  Lower `--process-width`, use `--corner-refine none`, and cap
processing with `--max-fps 5`.  The camera rate is set with `v4l2-ctl
--set-parm` (or left at the driver default).

**Brightness/exposure.**  The camera is converted to grayscale, so white
balance does not affect detection; exposure and ISO/gain do.  Use `v4l2-ctl
--list-ctrls` / `--set-ctrl` (and `--set-parm` if needed), or the `rpicam-vid`
options when piping.  Some drivers reset controls on a format change, so verify
them while the program is running.

## Tests

`make check32`, `make check64`, and `make check-amd64` run:

- no-argument help, `--help`, `--version`, `--list-presets`
- compact JSON schema and pose tolerances
- marker-size scaling and default 100 mm
- `--target-id`, empty maps, `--detect-only`, `--verbose`
- JSON `--timing.processMs`
- Aruco3 opt-in with `--aruco3`
- `--max-fps` validation
- multi-frame I420/auto-stdin confirmation and loss
- I420 odd-width/height rejection
- preset/manual conflict rejection
- all presets on matching blank frames
- all documented dictionaries
- ELF checks: static, ARMv6/VFPv2/no-NEON, AArch64, or x86-64

The tests are `tests/test_arucocam.py` plus `build/scripts/check.sh`; no camera
hardware is required.  `make tidy` runs clang-tidy inside the build image.

## License

MIT.  See [LICENSE](LICENSE).  Prebuilt binaries also include third-party
components; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Layout

```text
arucocam/
├── CMakeLists.txt
├── Makefile                    # image / pi32 / pi64 / amd64 / checks
├── build/
│   ├── docker/Dockerfile       # target-aware musl toolchain + static OpenCV
│   ├── cmake/toolchain/        # ARMv6, AArch64, and x86-64 toolchains
│   ├── scripts/                # build, check, ELF-check, tidy helpers
│   └── out/                    # generated per-target CMake build trees
├── dist/                       # generated release binaries
├── src/
│   ├── main.cpp                # CLI and frame loop
│   ├── calibration.*           # intrinsics and validation
│   ├── detector.*              # ArUco, fisheye PnP, confirmation
│   ├── frame_source.*          # image / stdin I420 / MJPEG
│   ├── v4l2_source.*           # Linux V4L2 mmap capture
│   └── output.*                # compact JSON and text
├── tests/
│   ├── gen_marker.cpp          # synthetic marker image
│   └── test_arucocam.py        # functional tests under qemu
├── .clang-format
├── .clang-tidy
├── .editorconfig
├── .github/workflows/ci.yml    # ARMv6 + ARM64 + x86-64 builds/releases
├── LICENSE
├── LICENSES/                   # third-party license texts
├── README.md
└── THIRD_PARTY_NOTICES.md
```
