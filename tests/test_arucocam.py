#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Functional tests for the compact arucocam output.

Runs the target binary under qemu, so it works for both ARMv6 and AArch64.
No third-party Python packages are required.
"""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

CAL_ARGS = [
    "--fx", "600", "--fy", "600", "--cx", "320", "--cy", "240",
    "--width", "640", "--height", "480",
]
ARUCOCAM_ARGS = CAL_ARGS + ["--marker-size", "100"]


class TestFailure(Exception):
    pass


def run_qemu(qemu, qemu_cpu, binary, args, data=None, timeout=60):
    cmd = [qemu]
    if qemu_cpu:
        cmd += ["-cpu", qemu_cpu]
    cmd += [str(binary)] + args
    return subprocess.run(
        cmd,
        input=data,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )


def check(proc, what):
    if proc.returncode != 0:
        raise TestFailure(
            f"{what}: exit {proc.returncode}\n"
            f"stdout: {proc.stdout.decode('utf-8', 'replace')}\n"
            f"stderr: {proc.stderr.decode('utf-8', 'replace')}"
        )
    return proc


def json_lines(text):
    return [json.loads(line) for line in text.splitlines() if line.strip()]


def read_pgm(path):
    data = Path(path).read_bytes()
    header, dims, maxval, pixels = data.split(b"\n", 3)
    if header != b"P5":
        raise TestFailure(f"{path}: expected binary PGM")
    width, height = map(int, dims.split())
    if int(maxval) != 255 or len(pixels) != width * height:
        raise TestFailure(f"{path}: bad PGM payload")
    return width, height, pixels


def i420_from_pgm(path):
    width, height, y = read_pgm(path)
    return y + b"\x80" * (width * height // 2)


def blank_i420(width=640, height=480):
    return b"\x00" * (width * height) + b"\x80" * (width * height // 2)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", required=True)
    parser.add_argument("--gen-marker", required=True)
    parser.add_argument("--qemu", default="qemu-arm")
    parser.add_argument("--qemu-cpu", default="max")
    args = parser.parse_args()

    if not Path(args.bin).is_file():
        raise TestFailure(f"binary not found: {args.bin}")
    if not Path(args.gen_marker).is_file():
        raise TestFailure(f"gen_marker not found: {args.gen_marker}")

    def arucocam(extra, data=None):
        return run_qemu(args.qemu, args.qemu_cpu, args.bin, extra, data)

    def gen(*extra):
        return run_qemu(args.qemu, args.qemu_cpu, args.gen_marker, list(extra))

    passed = []

    # No arguments -> help and exit 0.
    proc = check(arucocam([]), "no arguments")
    if b"Usage:" not in proc.stdout:
        raise TestFailure("no-argument output does not contain Usage:")
    help_proc = check(arucocam(["--help"]), "--help")
    if b"Usage:" not in help_proc.stdout:
        raise TestFailure("--help output does not contain Usage:")
    version_proc = check(arucocam(["--version"]), "--version")
    if b"arucocam " not in version_proc.stdout:
        raise TestFailure("--version output does not contain the program name")
    presets_proc = check(arucocam(["--list-presets"]), "--list-presets")
    presets_out = presets_proc.stdout
    if b"v1-640" not in presets_out or b"fisheye160-1920" not in presets_out:
        raise TestFailure("--list-presets output is missing presets")
    if b"--fx/--fy" not in presets_out:
        raise TestFailure("--list-presets output is missing its columns")
    if b"HFOV" not in presets_out:
        raise TestFailure("--list-presets output is missing HFOV")
    passed.append("no-argument help / --help / --version / --list-presets")

    with tempfile.TemporaryDirectory() as tmp:
        marker = Path(tmp) / "marker.pgm"
        check(gen(str(marker), "7", "240", "400", "240"), "gen_marker")
        blank = Path(tmp) / "blank.pgm"
        blank.write_bytes(b"P5\n640 480\n255\n" + b"\x00" * (640 * 480))

        base = arucocam(ARUCOCAM_ARGS + ["--image", str(marker), "--json"])
        check(base, "marker image")
        rows = json_lines(base.stdout.decode())
        if len(rows) != 1:
            raise TestFailure(f"expected one JSON line, got {len(rows)}")
        row = rows[0]
        if set(row) != {"7"}:
            raise TestFailure(f"expected key '7', got {sorted(row)}")
        marker_out = row["7"]
        if set(marker_out) != {"bearing", "horiz", "vert"}:
            raise TestFailure(f"unexpected marker fields: {sorted(marker_out)}")
        if abs(marker_out["bearing"] - 7.69) > 0.25:
            raise TestFailure(
                f"right-side bearing should be positive: {marker_out['bearing']}"
            )
        if abs(marker_out["horiz"] - 0.244) > 0.01:
            raise TestFailure(f"horiz out of tolerance: {marker_out['horiz']}")
        if abs(marker_out["vert"]) > 0.02:
            raise TestFailure(f"vert out of tolerance: {marker_out['vert']}")

        # Compass-style sign: a marker right of centre is +, left is -.
        left_marker = Path(tmp) / "marker-left.pgm"
        check(gen(str(left_marker), "8", "240", "240", "240"),
              "gen_marker left")
        left = arucocam(ARUCOCAM_ARGS + ["--image", str(left_marker), "--json"])
        check(left, "left marker image")
        left_rows = json_lines(left.stdout.decode())
        if not left_rows:
            raise TestFailure("left marker produced no JSON")
        left_row = left_rows[0].get("8")
        if left_row is None:
            raise TestFailure("left marker 8 missing from JSON")
        if abs(left_row["bearing"] - (-7.69)) > 0.25:
            raise TestFailure(
                f"left-side bearing should be negative: {left_row['bearing']}"
            )
        passed.append("bearing sign (right +, left -)")

        aruco3 = arucocam(ARUCOCAM_ARGS + ["--image", str(marker), "--json",
                                       "--aruco3"])
        check(aruco3, "Aruco3 detector")
        if set(json_lines(aruco3.stdout.decode())[0]) != {"7"}:
            raise TestFailure("--aruco3 did not detect marker 7")
        passed.append("JSON schema + pose / Aruco3")

        # Default marker size is 100 mm and must agree with an explicit 100.
        default = arucocam(CAL_ARGS + ["--image", str(marker), "--json"])
        check(default, "default marker size")
        default_row = json_lines(default.stdout.decode())[0]["7"]
        if abs(default_row["horiz"] - marker_out["horiz"]) > 0.002:
            raise TestFailure("default --marker-size is not 100 mm")
        passed.append("default marker size")

        # 120 mm must scale horiz by 1.2.
        size120 = arucocam(CAL_ARGS + ["--marker-size", "120",
                                     "--image", str(marker), "--json"])
        check(size120, "120 mm marker")
        size120_row = json_lines(size120.stdout.decode())[0]["7"]
        ratio = size120_row["horiz"] / marker_out["horiz"]
        if abs(ratio - 1.2) > 0.02:
            raise TestFailure(f"marker size scaling ratio {ratio:.3f}, expected 1.2")
        passed.append("marker size mm scaling")

        # target-id filter.
        target8 = arucocam(ARUCOCAM_ARGS + ["--image", str(marker),
                                        "--json", "--target-id", "8"])
        check(target8, "target-id 8")
        if json_lines(target8.stdout.decode()) != [{}]:
            raise TestFailure("target-id 8 should produce an empty map")
        target7 = arucocam(ARUCOCAM_ARGS + ["--image", str(marker),
                                        "--json", "--target-id", "7"])
        check(target7, "target-id 7")
        if set(json_lines(target7.stdout.decode())[0]) != {"7"}:
            raise TestFailure("target-id 7 should produce only key 7")
        passed.append("target-id filter")

        timed = arucocam(ARUCOCAM_ARGS + ["--image", str(marker), "--json",
                                      "--timing"])
        check(timed, "timing JSON")
        timed_row = json_lines(timed.stdout.decode())[0]
        if set(timed_row) != {"7", "_timing"}:
            raise TestFailure("--timing should add only _timing")
        if not isinstance(timed_row["_timing"].get("processMs"), (int, float)):
            raise TestFailure("--timing did not emit numeric processMs")
        passed.append("JSON timing")

        # Empty map for a blank image.
        empty = arucocam(ARUCOCAM_ARGS + ["--image", str(blank), "--json"])
        check(empty, "blank image")
        if json_lines(empty.stdout.decode()) != [{}]:
            raise TestFailure("blank image should produce an empty map")
        bad_dict = arucocam(ARUCOCAM_ARGS + ["--image", str(blank), "--dict", "NOPE"])
        if bad_dict.returncode == 0:
            raise TestFailure("unknown dictionary unexpectedly succeeded")
        passed.append("empty map")

        # --detect-only is text output and cannot be combined with --json.
        detect = arucocam(ARUCOCAM_ARGS + ["--image", str(marker), "--detect-only"])
        check(detect, "detect-only")
        if b"id=7" not in detect.stdout:
            raise TestFailure("detect-only did not report marker 7")
        bad_detect = arucocam(ARUCOCAM_ARGS + ["--image", str(marker),
                                           "--detect-only", "--json"])
        if bad_detect.returncode == 0:
            raise TestFailure("--detect-only --json should fail")
        verbose = arucocam(ARUCOCAM_ARGS + ["--image", str(marker), "--verbose"])
        check(verbose, "verbose text")
        if b"rms=" not in verbose.stdout:
            raise TestFailure("--verbose did not add RMS to text output")
        passed.append("detect-only / verbose")

        # Confirmation: three marker frames then one blank frame.
        marker_i420 = i420_from_pgm(marker)
        stream = marker_i420 * 3 + blank_i420()
        multi = arucocam(ARUCOCAM_ARGS + [
            "--stdin", "--stdin-format", "i420",
            "--width", "640", "--height", "480",
            "--confirm", "3", "--json",
        ], data=stream)
        check(multi, "multi-frame stdin")
        frames = json_lines(multi.stdout.decode())
        if len(frames) != 4:
            raise TestFailure(f"expected 4 frame outputs, got {len(frames)}")
        if frames[0] != {} or frames[1] != {}:
            raise TestFailure("marker appeared before --confirm 3")
        if set(frames[2]) != {"7"}:
            raise TestFailure("marker did not appear on the third frame")
        if frames[3] != {}:
            raise TestFailure("marker did not disappear after a missed frame")
        auto = arucocam(ARUCOCAM_ARGS + [
            "--stdin", "--stdin-format", "auto",
            "--confirm", "3", "--json",
        ], data=stream)
        check(auto, "auto stdin format")
        if json_lines(auto.stdout.decode()) != frames:
            raise TestFailure("auto stdin did not select raw I420")
        jpeg = Path(tmp) / "marker.jpg"
        check(gen(str(jpeg), "7", "240", "400", "240"), "gen_marker jpeg")
        jpeg_frame = arucocam(ARUCOCAM_ARGS + [
            "--stdin", "--stdin-format", "auto", "--confirm", "1", "--json",
        ], data=jpeg.read_bytes())
        check(jpeg_frame, "auto MJPEG stdin")
        if set(json_lines(jpeg_frame.stdout.decode())[0]) != {"7"}:
            raise TestFailure("auto stdin did not detect JPEG marker 7")
        passed.append("multi-frame confirmation / auto stdin")

    # CLI errors.
    error_cases = [
        (["--image", "x"], "missing --fx/--fy"),
        (["--preset", "does-not-exist"], "unknown preset"),
        (["--nope"], "unknown option"),
        (["--preset", "v2-640", "--fx", "500"], "preset plus manual calibration"),
        (["--preset", "v2-640", "--width", "800"], "preset plus width"),
        (ARUCOCAM_ARGS + ["--image", "x", "--max-fps", "-1"], "negative max-fps"),
        (ARUCOCAM_ARGS + ["--image", "x", "--verbose", "--json"], "verbose JSON"),
        (["--timing"], "timing without JSON"),
        (ARUCOCAM_ARGS + ["--image", "/nonexistent.pgm"], "missing image"),
        (ARUCOCAM_ARGS + [
            "--stdin", "--stdin-format", "i420",
            "--width", "641", "--height", "481",
        ], "odd I420 dimensions"),
    ]
    for argv, name in error_cases:
        proc = arucocam(argv)
        if proc.returncode == 0:
            raise TestFailure(f"{name} unexpectedly succeeded")
    passed.append("CLI errors")

    for name in passed:
        print(f"test: ok - {name}")
    print(f"test: {len(passed)} groups passed")


if __name__ == "__main__":
    try:
        main()
    except TestFailure as exc:
        print(f"test: FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
