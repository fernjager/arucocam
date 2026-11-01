// SPDX-License-Identifier: MIT

#pragma once

#include "detector.hpp"

#include <vector>

struct FrameReport {
    int target_filter = -1;  // restrict output to this ID when >= 0
    bool detect_only = false;
    bool verbose = false;
    bool timing = false;
    double process_ms = 0.0;
    std::vector<MarkerPose> markers;
};

void printSimpleJson(const FrameReport& report);
void printHuman(const FrameReport& report);
