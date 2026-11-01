// SPDX-License-Identifier: MIT

#include "calibration.hpp"

#include <opencv2/core.hpp>

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error("calibration: " + message);
}

}  // namespace

Calibration Calibration::forImageSize(int width, int height) const {
    if (width <= 0 || height <= 0) {
        fail("invalid image size");
    }
    if (width == image_width && height == image_height) {
        return *this;
    }

    const double expected = static_cast<double>(image_width) / image_height;
    const double actual = static_cast<double>(width) / height;
    if (std::fabs(expected - actual) > 0.005 * expected) {
        std::ostringstream oss;
        oss << "image is " << width << "x" << height
            << " but calibration is " << image_width << "x" << image_height
            << " (aspect mismatch); use a mode with the calibrated aspect ratio";
        fail(oss.str());
    }

    const double sx = static_cast<double>(width) / image_width;
    const double sy = static_cast<double>(height) / image_height;

    Calibration out = *this;
    out.image_width = width;
    out.image_height = height;
    out.K(0, 0) *= sx;
    out.K(1, 1) *= sy;
    out.K(0, 2) *= sx;
    out.K(1, 2) *= sy;
    return out;
}

cv::Mat Calibration::Dmat() const {
    cv::Mat d(4, 1, CV_64F);
    for (int i = 0; i < 4; ++i) {
        d.at<double>(i, 0) = D[i];
    }
    return d;
}

void Calibration::validate() const {
    if (image_width <= 0 || image_height <= 0) {
        fail("invalid image dimensions");
    }
    if (!cv::checkRange(cv::Mat(K))) {
        fail("fx/fy/cx/cy contain a non-finite value");
    }
    if (K(0, 0) <= 0.0 || K(1, 1) <= 0.0) {
        fail("fx and fy must be positive");
    }
    if (K(0, 2) < 0.0 || K(0, 2) > image_width ||
        K(1, 2) < 0.0 || K(1, 2) > image_height) {
        fail("cx/cy principal point is outside the image");
    }
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(D[i])) {
            fail("k1..k4 contain a non-finite value");
        }
    }
    if (!std::isfinite(marker_size_m) || marker_size_m <= 0.0) {
        fail("marker size must be positive");
    }
    if (!std::isfinite(max_rms_px) || max_rms_px <= 0.0) {
        fail("max RMS must be positive");
    }
    if (!std::isfinite(min_area_px) || min_area_px < 0.0) {
        fail("min area must be non-negative");
    }
    if (dictionary.empty()) {
        fail("dictionary must not be empty");
    }
}
