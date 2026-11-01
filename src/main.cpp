// SPDX-License-Identifier: MIT

#include "calibration.hpp"
#include "detector.hpp"
#include "frame_source.hpp"
#include "output.hpp"
#include "v4l2_source.hpp"

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#endif
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#ifndef ARUCOCAM_VERSION
#define ARUCOCAM_VERSION "dev"
#endif

namespace {

volatile std::sig_atomic_t g_stop = 0;

void onSignal(int) {
    g_stop = 1;
}

void installSignalHandlers() {
#if defined(__unix__) || defined(__APPLE__)
    struct sigaction action {};
    action.sa_handler = onSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;  // do not restart a blocking read on Ctrl-C
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
#else
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
#endif
}

void sleepUntil(int64_t deadline_ns) {
    // Sleep in short chunks so Ctrl-C is still responsive at low frame rates.
    while (!g_stop) {
        const int64_t now = monotonicNs();
        if (now >= deadline_ns) {
            break;
        }
        int64_t remaining = deadline_ns - now;
        if (remaining > 50'000'000LL) {
            remaining = 50'000'000LL;
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(remaining));
    }
}

struct Options {
    std::string image;
    bool stdin_source = false;
    std::string stdin_format = "auto";
    std::string device = "/dev/video0";
    bool device_set = false;

    // Camera calibration.  These are the only camera-specific inputs; there
    // is no config file.  cx/cy default to the image centre.
    double fx = 0.0;
    double fy = 0.0;
    double cx = -1.0;
    double cy = -1.0;
    double k1 = 0.0;
    double k2 = 0.0;
    double k3 = 0.0;
    double k4 = 0.0;
    int width = 640;
    int height = 480;
    bool preset_used = false;
    bool manual_calib = false;
    bool manual_res = false;

    double marker_size_mm = 100.0;
    std::string dictionary = "DICT_4X4_50";

    int process_width = 640;
    int max_fps = 0;
    int frames = 0;

    bool json = false;
    bool detect_only = false;
    bool aruco3 = false;
    bool verbose = false;
    bool timing = false;
    std::string corner_refine = "none";
    double max_rms = 5.0;
    double min_area = 50.0;
    int confirm = 3;
    bool confirm_set = false;
    int target_id = -1;

    bool help = false;
    bool version = false;
    bool list_presets = false;
};

struct PresetSpec {
    const char* name;
    int width;
    int height;
    double fx;
};

constexpr double kPi = 3.14159265358979323846;

const PresetSpec kPresets[] = {
    // Raspberry Pi Camera Module V1 / NoIR / Pi Zero camera (OV5647, 53.5 H)
    {"v1-640", 640, 480, 635.0},
    {"v1-1280", 1280, 720, 1270.0},
    {"v1-1920", 1920, 1080, 1905.0},
    // Camera Module V2 / NoIR (IMX219, 62.2 H)
    {"v2-640", 640, 480, 530.0},
    {"v2-1280", 1280, 720, 1061.0},
    {"v2-1920", 1920, 1080, 1591.0},
    // Camera Module 3 standard (IMX708, 66 H)
    {"cm3-640", 640, 480, 493.0},
    {"cm3-1280", 1280, 720, 985.0},
    {"cm3-1920", 1920, 1080, 1478.0},
    // Camera Module 3 Wide (IMX708, 102 H)
    {"cm3wide-640", 640, 480, 259.0},
    {"cm3wide-1280", 1280, 720, 518.0},
    {"cm3wide-1920", 1920, 1080, 777.0},
    // AI Camera (IMX500, 66.3 H)
    {"aicam-640", 640, 480, 490.0},
    {"aicam-1280", 1280, 720, 980.0},
    {"aicam-1920", 1920, 1080, 1470.0},
    // Arducam 16MP IMX519 autofocus, stock 66 H lens
    {"imx519-640", 640, 480, 493.0},
    {"imx519-1280", 1280, 720, 985.0},
    {"imx519-1920", 1920, 1080, 1478.0},
    // Arducam 64MP Hawk-Eye autofocus, stock 72 H lens
    {"hawkeye-640", 640, 480, 440.0},
    {"hawkeye-1280", 1280, 720, 881.0},
    {"hawkeye-1920", 1920, 1080, 1321.0},
    // Arducam 64MP OwlSight, 6.65 mm 68 H lens
    {"owlsight-640", 640, 480, 474.0},
    {"owlsight-1280", 1280, 720, 949.0},
    {"owlsight-1920", 1920, 1080, 1423.0},
    // 160-degree diagonal OV5647 fisheye (Waveshare G/H class)
    {"fisheye160-640", 640, 480, 286.0},
    {"fisheye160-1280", 1280, 720, 573.0},
    {"fisheye160-1920", 1920, 1080, 860.0},
};

void applyPreset(const std::string& name, Options& o) {
    // Starting points, not calibrations.  Standard modules use the pinhole
    // focal from their horizontal FOV; the 160-degree fisheye uses the
    // equidistant estimate from its 160-degree diagonal FOV.  k = 0.
    for (const auto& preset : kPresets) {
        if (name == preset.name) {
            o.width = preset.width;
            o.height = preset.height;
            o.fx = o.fy = preset.fx;
            o.cx = preset.width / 2.0;
            o.cy = preset.height / 2.0;
            o.k1 = o.k2 = o.k3 = o.k4 = 0.0;
            return;
        }
    }
    throw std::runtime_error(
        "unknown preset '" + name +
        "' (use v1/v2/cm3/cm3wide/aicam/fisheye160, or "
        "imx519/hawkeye/owlsight with -640, -1280, or -1920)");
}

double horizontalFov(const PresetSpec& preset) {
    if (std::string(preset.name).rfind("fisheye160-", 0) == 0) {
        // Equidistant fisheye model: r = fx * theta.
        return (preset.width / preset.fx) * 180.0 / kPi;
    }
    return 2.0 * std::atan(preset.width / (2.0 * preset.fx)) * 180.0 / kPi;
}

void listPresets(std::ostream& out) {
    out << std::left << std::setw(17) << "Preset"
        << std::right << std::setw(9) << "--width"
        << std::setw(9) << "--height"
        << std::setw(10) << "--fx/--fy"
        << std::setw(8) << "HFOV"
        << std::setw(6) << "--cx"
        << std::setw(6) << "--cy" << '\n';
    for (const auto& preset : kPresets) {
        out << std::left << std::setw(17) << preset.name
            << std::right << std::setw(9) << preset.width
            << std::setw(9) << preset.height
            << std::setw(10) << std::fixed << std::setprecision(1) << preset.fx
            << std::setw(8) << horizontalFov(preset)
            << std::setw(6) << preset.width / 2
            << std::setw(6) << preset.height / 2 << '\n';
    }
    out << "\nHFOV is horizontal.  fisheye160 is 160 degrees diagonal "
           "(about 128 degrees horizontal).\n"
        << "k1..k4 are 0; --marker-size is not set by a preset.\n";
}

void usage(std::ostream& out) {
    out << "arucocam " << ARUCOCAM_VERSION
        << " -- ArUco marker bearing and distance from a camera\n\n"
        << "Usage:\n"
        << "  arucocam --preset fisheye160-640 --device /dev/video0 [options]\n"
        << "  arucocam --preset fisheye160-640 --stdin --stdin-format i420\n"
        << "  arucocam --preset fisheye160-640 --image FRAME.pgm --json\n\n"
        << "Calibration:\n"
        << "  --preset NAME         starting point for a common camera:\n"
        << "                        v1/v2/cm3/cm3wide/aicam/fisheye160\n"
        << "                        imx519/hawkeye/owlsight + -640/-1280/-1920\n"
        << "  --fx PX               Focal length X (required unless --preset)\n"
        << "  --fy PX               Focal length Y (required unless --preset)\n"
        << "  --cx PX               Principal point X; default width/2\n"
        << "  --cy PX               Principal point Y; default height/2\n"
        << "  --k1 K --k2 K --k3 K --k4 K   Fisheye distortion; default 0\n"
        << "  --width N             Image/capture width; default 640\n"
        << "  --height N            Image/capture height; default 480\n"
        << "                        (not with --preset; preset sets these)\n\n"
        << "Source:\n"
        << "  --image PATH          One still image (PGM, PNG, or JPEG)\n"
        << "  --stdin               Read a frame stream from standard input\n"
        << "  --stdin-format FMT    auto (default), mjpeg, i420/yuv420, or gray\n"
        << "  --device PATH         V4L2 device (default /dev/video0)\n"
        << "  --max-fps N           Cap processed frames/sec; 0 = unlimited\n\n"
        << "Detection:\n"
        << "  --marker-size MM      Marker black-square size, millimetres (default 100)\n"
        << "  --dict NAME           ArUco dictionary (default DICT_4X4_50; see README)\n"
        << "  --process-width N     Downscale before detection; 0 = full size\n"
        << "  --corner-refine MODE  none (default), subpix, contour, apriltag\n"
        << "  --max-rms PX          Reject poses above this reprojection RMS\n"
        << "  --min-area PX2        Reject markers smaller than this\n"
        << "  --detect-only         Skip PnP; report marker IDs and pixel centres\n"
        << "  --aruco3              Enable Aruco3 (test at your resolution; see README)\n\n"
        << "Output:\n"
        << "  --target-id N         Restrict output to this marker ID\n"
        << "  --confirm N           Valid frames before a marker is confirmed\n"
        << "  --json                Compact map: {id:{bearing,horiz,vert}} per frame\n"
        << "  --verbose             Add RMS and area to text output\n"
        << "  --timing              Add _timing.processMs to JSON output\n"
        << "  --frames N            Stop after N processed frames; 0 = until EOF/signal\n\n"
        << "  --list-presets        Show preset names, intrinsics, and HFOV\n"
        << "  --help                Show this help\n"
        << "  --version             Show the version\n\n"
        << "Coordinates (body frame: x forward, y left, z up):\n"
        << "  bearing   degrees, positive right (clockwise), -180 to 180\n"
        << "  horiz     horizontal distance in metres\n"
        << "  vert      vertical offset in metres, positive up\n";
}

std::string valueFor(int& i, int argc, char** argv, const std::string& arg) {
    const auto eq = arg.find('=');
    if (eq != std::string::npos) {
        return arg.substr(eq + 1);
    }
    if (i + 1 >= argc) {
        throw std::runtime_error("missing value for " + arg);
    }
    return argv[++i];
}

int parseInt(const std::string& text, const std::string& what) {
    try {
        size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return value;
    } catch (const std::exception&) {
        throw std::runtime_error("invalid " + what + " '" + text + "'");
    }
}

double parseDouble(const std::string& text, const std::string& what) {
    try {
        size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(value)) {
            throw std::invalid_argument("invalid number");
        }
        return value;
    } catch (const std::exception&) {
        throw std::runtime_error("invalid " + what + " '" + text + "'");
    }
}

Options parseArgs(int argc, char** argv) {
    Options o;
    bool positional_image = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto eq = arg.find('=');
        const std::string name = eq == std::string::npos ? arg : arg.substr(0, eq);

        if (name == "--help" || name == "-h") {
            o.help = true;
        } else if (name == "--version") {
            o.version = true;
        } else if (name == "--list-presets") {
            o.list_presets = true;
        } else if (name == "--json") {
            o.json = true;
        } else if (name == "--stdin") {
            o.stdin_source = true;
        } else if (name == "--detect-only") {
            o.detect_only = true;
        } else if (name == "--aruco3") {
            o.aruco3 = true;
        } else if (name == "--verbose") {
            o.verbose = true;
        } else if (name == "--timing") {
            o.timing = true;
        } else if (name == "--preset") {
            applyPreset(valueFor(i, argc, argv, arg), o);
            o.preset_used = true;
        } else if (name == "--fx") {
            o.fx = parseDouble(valueFor(i, argc, argv, arg), "fx");
            o.manual_calib = true;
        } else if (name == "--fy") {
            o.fy = parseDouble(valueFor(i, argc, argv, arg), "fy");
            o.manual_calib = true;
        } else if (name == "--cx") {
            o.cx = parseDouble(valueFor(i, argc, argv, arg), "cx");
            o.manual_calib = true;
        } else if (name == "--cy") {
            o.cy = parseDouble(valueFor(i, argc, argv, arg), "cy");
            o.manual_calib = true;
        } else if (name == "--k1") {
            o.k1 = parseDouble(valueFor(i, argc, argv, arg), "k1");
            o.manual_calib = true;
        } else if (name == "--k2") {
            o.k2 = parseDouble(valueFor(i, argc, argv, arg), "k2");
            o.manual_calib = true;
        } else if (name == "--k3") {
            o.k3 = parseDouble(valueFor(i, argc, argv, arg), "k3");
            o.manual_calib = true;
        } else if (name == "--k4") {
            o.k4 = parseDouble(valueFor(i, argc, argv, arg), "k4");
            o.manual_calib = true;
        } else if (name == "--image") {
            o.image = valueFor(i, argc, argv, arg);
        } else if (name == "--stdin-format") {
            o.stdin_format = valueFor(i, argc, argv, arg);
        } else if (name == "--device") {
            o.device = valueFor(i, argc, argv, arg);
            o.device_set = true;
        } else if (name == "--marker-size") {
            o.marker_size_mm = parseDouble(valueFor(i, argc, argv, arg), "marker size (mm)");
        } else if (name == "--dict") {
            o.dictionary = valueFor(i, argc, argv, arg);
        } else if (name == "--process-width") {
            o.process_width = parseInt(valueFor(i, argc, argv, arg), "process width");
        } else if (name == "--max-fps") {
            o.max_fps = parseInt(valueFor(i, argc, argv, arg), "max fps");
        } else if (name == "--frames") {
            o.frames = parseInt(valueFor(i, argc, argv, arg), "frame count");
        } else if (name == "--width") {
            o.width = parseInt(valueFor(i, argc, argv, arg), "width");
            o.manual_res = true;
        } else if (name == "--height") {
            o.height = parseInt(valueFor(i, argc, argv, arg), "height");
            o.manual_res = true;
        } else if (name == "--corner-refine") {
            o.corner_refine = valueFor(i, argc, argv, arg);
        } else if (name == "--max-rms") {
            o.max_rms = parseDouble(valueFor(i, argc, argv, arg), "max RMS");
        } else if (name == "--min-area") {
            o.min_area = parseDouble(valueFor(i, argc, argv, arg), "min area");
        } else if (name == "--confirm") {
            o.confirm = parseInt(valueFor(i, argc, argv, arg), "confirm frames");
            o.confirm_set = true;
        } else if (name == "--target-id") {
            o.target_id = parseInt(valueFor(i, argc, argv, arg), "target ID");
        } else if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("unknown option '" + arg + "'");
        } else if (!positional_image) {
            o.image = arg;
            positional_image = true;
        } else {
            throw std::runtime_error("unexpected argument '" + arg + "'");
        }
    }

    return o;
}

void validateOptions(const Options& o) {
    if (o.preset_used && o.manual_calib) {
        throw std::runtime_error(
            "--preset cannot be combined with --fx/--fy/--cx/--cy/--k1..--k4");
    }
    if (o.preset_used && o.manual_res) {
        throw std::runtime_error(
            "--preset sets --width/--height; do not combine them");
    }
    if (o.process_width < 0) {
        throw std::runtime_error("--process-width must be non-negative");
    }
    if (o.max_fps < 0) {
        throw std::runtime_error("--max-fps must be non-negative");
    }
    if (o.frames < 0) {
        throw std::runtime_error("--frames must be non-negative");
    }
    if (o.width <= 0 || o.height <= 0) {
        throw std::runtime_error("--width and --height must be positive");
    }
    if (o.fx <= 0.0 || o.fy <= 0.0) {
        throw std::runtime_error("--fx and --fy are required and must be positive");
    }
    if (o.cx < -1.0 || o.cy < -1.0) {
        throw std::runtime_error("--cx and --cy must be non-negative (or omitted)");
    }
    if (o.confirm < 1) {
        throw std::runtime_error("--confirm must be at least 1");
    }
    if (o.target_id < -1) {
        throw std::runtime_error("--target-id must be -1 or a marker ID");
    }
    if (o.detect_only && o.json) {
        throw std::runtime_error(
            "--detect-only has no bearing/horiz/vert; drop --json");
    }
    if (o.verbose && o.json) {
        throw std::runtime_error("--verbose is text-only; drop --json");
    }
    if (o.timing && !o.json) {
        throw std::runtime_error("--timing requires --json");
    }
    if (o.marker_size_mm <= 0.0) {
        throw std::runtime_error("--marker-size must be positive millimetres");
    }
    if (o.max_rms <= 0.0) {
        throw std::runtime_error("--max-rms must be positive");
    }
    if (o.min_area < 0.0) {
        throw std::runtime_error("--min-area must be non-negative");
    }
    if (o.dictionary.empty()) {
        throw std::runtime_error("--dict must not be empty");
    }

    const int sources = static_cast<int>(!o.image.empty()) +
                        static_cast<int>(o.stdin_source) +
                        static_cast<int>(o.device_set);
    if (sources > 1) {
        throw std::runtime_error("choose only one of --image, --stdin, --device");
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            usage(std::cout);
            return 0;
        }
        const Options options = parseArgs(argc, argv);
        if (options.help) {
            usage(std::cout);
            return 0;
        }
        if (options.version) {
            std::cout << "arucocam " << ARUCOCAM_VERSION << '\n';
            return 0;
        }
        if (options.list_presets) {
            listPresets(std::cout);
            return 0;
        }
        validateOptions(options);

        Calibration calibration;
        calibration.image_width = options.width;
        calibration.image_height = options.height;
        const double cx = options.cx >= 0.0 ? options.cx : options.width / 2.0;
        const double cy = options.cy >= 0.0 ? options.cy : options.height / 2.0;
        calibration.K = cv::Matx33d(options.fx, 0.0, cx,
                                    0.0, options.fy, cy,
                                    0.0, 0.0, 1.0);
        calibration.D = cv::Vec4d(options.k1, options.k2, options.k3, options.k4);
        calibration.marker_size_m = options.marker_size_mm / 1000.0;
        calibration.dictionary = options.dictionary;
        calibration.max_rms_px = options.max_rms;
        calibration.min_area_px = options.min_area;
        calibration.validate();

        const int capture_width = options.width;
        const int capture_height = options.height;

        std::unique_ptr<FrameSource> source;
        if (!options.image.empty()) {
            source = makeImageSource(options.image);
        } else if (options.stdin_source) {
            source = makeStdinSource(options.stdin_format, capture_width,
                                     capture_height);
        } else {
            source = makeV4L2Source(options.device, capture_width, capture_height);
        }

        DetectorOptions detector_options;
        detector_options.process_width = options.process_width;
        detector_options.confirm_frames =
            (!options.image.empty() && !options.confirm_set) ? 1 : options.confirm;
        detector_options.detect_only = options.detect_only;
        detector_options.use_aruco3 = options.aruco3;
        detector_options.corner_refine = options.corner_refine;

        MarkerDetector detector(calibration, detector_options);

        installSignalHandlers();

        uint64_t sequence = 0;
        int64_t next_process_ns = 0;
        Frame frame;

        while (!g_stop) {
            if (options.frames > 0 &&
                sequence >= static_cast<uint64_t>(options.frames)) {
                break;
            }
            if (!source->next(frame)) {
                if (source->isLive() && !g_stop) {
                    continue;  // select() timeout, not end of stream
                }
                break;
            }
            ++sequence;

            if (frame.gray.empty()) {
                continue;
            }

            const int64_t begin_ns = monotonicNs();
            FrameResult result;
            try {
                result = detector.detect(frame.gray);
            } catch (const cv::Exception& e) {
                std::cerr << "arucocam: frame " << sequence << ": " << e.what()
                          << '\n';
                continue;
            }
            const int64_t end_ns = monotonicNs();

            FrameReport report;
            report.target_filter = options.target_id;
            report.detect_only = options.detect_only;
            report.verbose = options.verbose;
            report.timing = options.timing;
            report.process_ms = static_cast<double>(end_ns - begin_ns) / 1e6;
            report.markers = std::move(result.markers);

            if (options.json) {
                printSimpleJson(report);
            } else {
                printHuman(report);
            }
            std::cout.flush();

            if (options.frames > 0 &&
                sequence >= static_cast<uint64_t>(options.frames)) {
                break;
            }
            if (options.max_fps > 0) {
                const int64_t interval_ns =
                    1'000'000'000LL / static_cast<int64_t>(options.max_fps);
                const int64_t now_ns = monotonicNs();
                if (next_process_ns == 0) {
                    next_process_ns = begin_ns + interval_ns;
                } else {
                    next_process_ns += interval_ns;
                }
                if (next_process_ns < now_ns) {
                    next_process_ns = now_ns;
                }
                sleepUntil(next_process_ns);
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "arucocam: error: " << e.what() << '\n';
        return 1;
    }
}
