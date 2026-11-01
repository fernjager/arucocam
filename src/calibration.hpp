// SPDX-License-Identifier: MIT

#pragma once

#include <opencv2/core.hpp>

#include <string>

// Camera intrinsics and the fixed configuration that belongs with them.
//
// Everything is supplied on the command line; there is no config file.  The
// struct is intentionally plain so the same binary can be pointed at any
// camera by changing flags.
struct Calibration {
    int image_width = 0;
    int image_height = 0;

    // Fisheye pinhole intrinsics: fx, 0, cx / 0, fy, cy / 0, 0, 1.
    cv::Matx33d K = cv::Matx33d::eye();
    // Fisheye distortion: k1, k2, k3, k4.
    cv::Vec4d D{0.0, 0.0, 0.0, 0.0};

    double marker_size_m = 0.10;   // outer black square, metres
    std::string dictionary = "DICT_4X4_50";

    // Detection gates.
    double max_rms_px = 5.0;
    double min_area_px = 50.0;

    // Return a copy whose K matches an image of this size.  The aspect ratio
    // must match the calibrated image; a different mode is not a crop.
    Calibration forImageSize(int width, int height) const;

    cv::Mat Dmat() const;

    void validate() const;
};
