// SPDX-License-Identifier: MIT

#pragma once

#include <opencv2/core.hpp>

#include <cstdint>
#include <memory>
#include <string>

struct Frame {
    cv::Mat gray;  // CV_8UC1
};

class FrameSource {
public:
    virtual ~FrameSource() = default;
    virtual bool next(Frame& frame) = 0;
    // Live sources keep running after a timeout; file/pipe sources end at EOF.
    virtual bool isLive() const { return false; }
};

std::unique_ptr<FrameSource> makeImageSource(const std::string& path);

// format: "mjpeg", "i420" (a.k.a. yuv420), or "gray".
std::unique_ptr<FrameSource> makeStdinSource(const std::string& format,
                                             int width,
                                             int height);

std::unique_ptr<FrameSource> makeV4L2Source(const std::string& device,
                                            int width,
                                            int height);

int64_t monotonicNs();
