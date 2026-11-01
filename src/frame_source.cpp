// SPDX-License-Identifier: MIT

#include "frame_source.hpp"

#include "v4l2_source.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)
#include <time.h>
#endif

int64_t monotonicNs() {
#if defined(__linux__) || defined(__APPLE__)
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        throw std::runtime_error(std::string("clock_gettime: ") + std::strerror(errno));
    }
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL +
           static_cast<int64_t>(ts.tv_nsec);
#else
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
#endif
}

namespace {

std::string lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

class ImageSource : public FrameSource {
public:
    explicit ImageSource(std::string path) : path_(std::move(path)) {
        frame_.gray = cv::imread(path_, cv::IMREAD_GRAYSCALE);
        if (frame_.gray.empty()) {
            throw std::runtime_error("cannot read image '" + path_ +
                                     "' (PGM/PNG/JPEG)");
        }
        if (frame_.gray.type() != CV_8UC1) {
            throw std::runtime_error("image is not 8-bit grayscale");
        }
    }

    bool next(Frame& frame) override {
        if (used_) {
            return false;
        }
        used_ = true;
        frame = frame_;
        return true;
    }

private:
    std::string path_;
    Frame frame_;
    bool used_ = false;
};

class StdinRawSource : public FrameSource {
public:
    StdinRawSource(bool i420, int width, int height)
        : i420_(i420), width_(width), height_(height) {
        if (width_ <= 0 || height_ <= 0) {
            throw std::runtime_error("stdin raw formats need --width and --height");
        }
        if (i420_ && ((width_ % 2) != 0 || (height_ % 2) != 0)) {
            throw std::runtime_error(
                "I420 stdin needs even --width and --height");
        }
        const size_t pixels = static_cast<size_t>(width_) * height_;
        bytes_per_frame_ = i420_ ? pixels * 3 / 2 : pixels;
    }

    bool next(Frame& frame) override {
        std::vector<unsigned char> buffer(bytes_per_frame_);
        if (!readExact(buffer.data(), buffer.size())) {
            return false;
        }
        frame.gray = cv::Mat(height_, width_, CV_8UC1);
        std::memcpy(frame.gray.data, buffer.data(),
                    static_cast<size_t>(width_) * height_);
        return true;
    }

private:
    bool readExact(unsigned char* dst, size_t size) {
        size_t offset = 0;
        while (offset < size) {
            std::cin.read(reinterpret_cast<char*>(dst + offset),
                          static_cast<std::streamsize>(size - offset));
            const std::streamsize got = std::cin.gcount();
            if (got <= 0) {
                return false;
            }
            offset += static_cast<size_t>(got);
        }
        return true;
    }

    bool i420_ = false;
    int width_ = 0;
    int height_ = 0;
    size_t bytes_per_frame_ = 0;
};

class StdinMjpegSource : public FrameSource {
public:
    bool next(Frame& frame) override {
        const unsigned char soi[2] = {0xFF, 0xD8};
        const unsigned char eoi[2] = {0xFF, 0xD9};

        for (;;) {
            auto start = std::search(buffer_.begin(), buffer_.end(), soi, soi + 2);
            if (start == buffer_.end()) {
                // No JPEG start in the buffer.  Keep a trailing 0xFF: it may be
                // the first half of a start marker split across reads.
                if (!buffer_.empty() && buffer_.back() == 0xFF) {
                    buffer_.erase(buffer_.begin(), buffer_.end() - 1);
                } else {
                    buffer_.clear();
                }
            } else if (start != buffer_.begin()) {
                buffer_.erase(buffer_.begin(), start);
            }

            if (buffer_.size() >= 2) {
                auto end = std::search(buffer_.begin() + 2, buffer_.end(),
                                       eoi, eoi + 2);
                if (end != buffer_.end()) {
                    const size_t length =
                        static_cast<size_t>(end - buffer_.begin()) + 2;
                    cv::Mat encoded(1, static_cast<int>(length), CV_8UC1,
                                    buffer_.data());
                    cv::Mat decoded;
                    try {
                        decoded = cv::imdecode(encoded, cv::IMREAD_GRAYSCALE);
                    } catch (const cv::Exception&) {
                        decoded.release();
                    }
                    buffer_.erase(buffer_.begin(), buffer_.begin() + length);
                    if (!decoded.empty()) {
                        frame.gray = decoded;
                        return true;
                    }
                    continue;
                }
            }

            if (buffer_.size() > 32U * 1024U * 1024U) {
                throw std::runtime_error("MJPEG frame exceeds 32 MiB");
            }
            if (!readMore()) {
                return false;
            }
        }
    }

private:
    bool readMore() {
        char chunk[64 * 1024];
        std::cin.read(chunk, sizeof(chunk));
        const std::streamsize got = std::cin.gcount();
        if (got <= 0) {
            return false;
        }
        buffer_.insert(buffer_.end(),
                       reinterpret_cast<unsigned char*>(chunk),
                       reinterpret_cast<unsigned char*>(chunk) + got);
        return true;
    }

    std::vector<unsigned char> buffer_;
};

}  // namespace

std::unique_ptr<FrameSource> makeImageSource(const std::string& path) {
    return std::make_unique<ImageSource>(path);
}

std::unique_ptr<FrameSource> makeStdinSource(const std::string& format,
                                             int width,
                                             int height) {
    const std::string f = lower(format);
    if (f == "auto") {
        // JPEG starts with 0xFFD8.  Check two bytes: a raw Y plane can also
        // start with 0xFF.  Anything that is not JPEG is assumed to be raw
        // I420, which is the cheap path for YUV producers.
        const int first = std::cin.peek();
        if (first == 0xFF) {
            std::cin.get();
            const int second = std::cin.peek();
            if (!std::cin.putback(static_cast<char>(first))) {
                throw std::runtime_error("could not inspect stdin format");
            }
            if (second == 0xD8) {
                return std::make_unique<StdinMjpegSource>();
            }
        }
        return std::make_unique<StdinRawSource>(true, width, height);
    }
    if (f == "mjpeg" || f == "jpeg") {
        return std::make_unique<StdinMjpegSource>();
    }
    if (f == "i420" || f == "yuv420" || f == "yuv") {
        return std::make_unique<StdinRawSource>(true, width, height);
    }
    if (f == "gray" || f == "grey" || f == "raw") {
        return std::make_unique<StdinRawSource>(false, width, height);
    }
    throw std::runtime_error("unknown stdin format '" + format +
                             "' (use mjpeg, i420, or gray)");
}
