// SPDX-License-Identifier: MIT

#include "detector.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace {

std::string upper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

cv::aruco::PredefinedDictionaryType dictionaryType(const std::string& name) {
    const std::string n = upper(name);
    if (n == "DICT_4X4_50") return cv::aruco::DICT_4X4_50;
    if (n == "DICT_4X4_100") return cv::aruco::DICT_4X4_100;
    if (n == "DICT_4X4_250") return cv::aruco::DICT_4X4_250;
    if (n == "DICT_4X4_1000") return cv::aruco::DICT_4X4_1000;
    if (n == "DICT_5X5_50") return cv::aruco::DICT_5X5_50;
    if (n == "DICT_5X5_100") return cv::aruco::DICT_5X5_100;
    if (n == "DICT_5X5_250") return cv::aruco::DICT_5X5_250;
    if (n == "DICT_5X5_1000") return cv::aruco::DICT_5X5_1000;
    if (n == "DICT_6X6_50") return cv::aruco::DICT_6X6_50;
    if (n == "DICT_6X6_100") return cv::aruco::DICT_6X6_100;
    if (n == "DICT_6X6_250") return cv::aruco::DICT_6X6_250;
    if (n == "DICT_6X6_1000") return cv::aruco::DICT_6X6_1000;
    if (n == "DICT_7X7_50") return cv::aruco::DICT_7X7_50;
    if (n == "DICT_7X7_100") return cv::aruco::DICT_7X7_100;
    if (n == "DICT_7X7_250") return cv::aruco::DICT_7X7_250;
    if (n == "DICT_7X7_1000") return cv::aruco::DICT_7X7_1000;
    if (n == "DICT_ARUCO_ORIGINAL") return cv::aruco::DICT_ARUCO_ORIGINAL;
    if (n == "DICT_APRILTAG_16H5") return cv::aruco::DICT_APRILTAG_16h5;
    if (n == "DICT_APRILTAG_25H9") return cv::aruco::DICT_APRILTAG_25h9;
    if (n == "DICT_APRILTAG_36H10") return cv::aruco::DICT_APRILTAG_36h10;
    if (n == "DICT_APRILTAG_36H11") return cv::aruco::DICT_APRILTAG_36h11;
    throw std::runtime_error("unknown ArUco dictionary '" + name + "'");
}

cv::aruco::CornerRefineMethod refinement(const std::string& name) {
    const std::string n = upper(name);
    if (n == "NONE") return cv::aruco::CORNER_REFINE_NONE;
    if (n == "SUBPIX") return cv::aruco::CORNER_REFINE_SUBPIX;
    if (n == "CONTOUR") return cv::aruco::CORNER_REFINE_CONTOUR;
    if (n == "APRILTAG") return cv::aruco::CORNER_REFINE_APRILTAG;
    throw std::runtime_error("unknown corner refinement '" + name + "'");
}

}  // namespace

MarkerDetector::MarkerDetector(const Calibration& calibration,
                               const DetectorOptions& options)
    : calibration_(calibration),
      options_(options),
      dictionary_(cv::aruco::getPredefinedDictionary(
          dictionaryType(calibration.dictionary))) {
    if (options_.process_width < 0) {
        throw std::runtime_error("process width must be non-negative");
    }
    if (options_.confirm_frames < 1) {
        throw std::runtime_error("confirm frames must be at least 1");
    }

    cv::aruco::DetectorParameters params;
    params.cornerRefinementMethod = refinement(options_.corner_refine);
    // The defaults are tuned for a normal webcam at VGA.  These keep small,
    // distant markers in the candidate set without opening the door to noise.
    params.minMarkerPerimeterRate = 0.02;
    params.maxMarkerPerimeterRate = 4.0;
    params.polygonalApproxAccuracyRate = 0.05;
    params.minCornerDistanceRate = 0.03;
    params.minDistanceToBorder = 2;
    // Aruco3 uses a smaller canonical image for the contour search.  It is
    // the main CPU saving on the Pi Zero; the marker must still be large
    // enough to pass minMarkerPerimeterRate.
    params.useAruco3Detection = options_.use_aruco3;
    detector_ = std::make_unique<cv::aruco::ArucoDetector>(dictionary_, params);

    const float half = static_cast<float>(calibration_.marker_size_m * 0.5);
    object_points_ = {
        cv::Point3f(-half, half, 0.0F),
        cv::Point3f(half, half, 0.0F),
        cv::Point3f(half, -half, 0.0F),
        cv::Point3f(-half, -half, 0.0F),
    };
    identity_K_ = cv::Mat::eye(3, 3, CV_64F);
    zero_dist_ = cv::Mat::zeros(4, 1, CV_64F);
}

MarkerPose MarkerDetector::solveOne(const std::vector<cv::Point2f>& corners,
                                    int id,
                                    const cv::Mat& K,
                                    const cv::Mat& D,
                                    double area_px) {
    MarkerPose p;
    p.id = id;
    p.area_px = area_px;
    p.center_px = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25F;

    std::vector<cv::Point2f> normalized;
    cv::fisheye::undistortPoints(corners, normalized, K, D);
    if (normalized.size() != 4) {
        return p;
    }

    cv::Mat rvec;
    cv::Mat tvec;
    bool ok = false;
    try {
        ok = cv::solvePnP(object_points_, normalized, identity_K_, zero_dist_,
                          rvec, tvec, false, cv::SOLVEPNP_IPPE_SQUARE);
        if (!ok) {
            ok = cv::solvePnP(object_points_, normalized, identity_K_, zero_dist_,
                              rvec, tvec, false, cv::SOLVEPNP_ITERATIVE);
        }
    } catch (const cv::Exception&) {
        ok = false;
    }
    if (!ok || rvec.rows != 3 || tvec.rows != 3) {
        return p;
    }

    // Back-project with the real fisheye model.  This is the gate that matters:
    // a pose that does not reproduce the four raw corners cannot be trusted.
    std::vector<cv::Point2f> projected;
    cv::fisheye::projectPoints(object_points_, projected, rvec, tvec, K, D);
    double sum_sq = 0.0;
    for (size_t i = 0; i < 4; ++i) {
        const cv::Point2f d = projected[i] - corners[i];
        sum_sq += static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y;
    }
    p.rms_px = std::sqrt(sum_sq / 4.0);

    const cv::Vec3d t(tvec.at<double>(0, 0),
                      tvec.at<double>(1, 0),
                      tvec.at<double>(2, 0));
    const double range = cv::norm(t);
    if (!std::isfinite(range) || range < 0.001 || range > 100.0 ||
        t[2] <= 0.0 || p.rms_px > calibration_.max_rms_px ||
        area_px < calibration_.min_area_px) {
        return p;
    }

    // Optical frame (x right, y down, z forward) -> body frame
    // (x forward, y left, z up).
    const double forward = t[2];
    const double left = -t[0];
    const double up = -t[1];
    const double planar = std::hypot(forward, left);
    if (!std::isfinite(planar)) {
        return p;
    }

    p.bearing_rad = std::atan2(left, forward);
    p.planar_range_m = planar;
    p.up_m = up;
    p.pose_valid = true;
    return p;
}

FrameResult MarkerDetector::detect(const cv::Mat& gray) {
    FrameResult result;

    if (gray.empty()) {
        return result;
    }

    if (gray.cols != calibrated_for_width_ || gray.rows != calibrated_for_height_) {
        frame_calibration_ = calibration_.forImageSize(gray.cols, gray.rows);
        calibrated_for_width_ = gray.cols;
        calibrated_for_height_ = gray.rows;
        frame_D_ = frame_calibration_.Dmat();
        cached_scale_ = -1.0;
    }

    cv::Mat mono;
    if (gray.channels() == 1) {
        mono = gray;
    } else if (gray.channels() == 3) {
        cv::cvtColor(gray, mono, cv::COLOR_BGR2GRAY);
    } else if (gray.channels() == 4) {
        cv::cvtColor(gray, mono, cv::COLOR_BGRA2GRAY);
    } else {
        throw std::runtime_error("unsupported input image channel count");
    }

    cv::Mat processed;
    double scale = 1.0;
    if (options_.process_width > 0 && mono.cols > options_.process_width) {
        scale = static_cast<double>(options_.process_width) / mono.cols;
        const int pw = options_.process_width;
        const int ph = std::max(1, static_cast<int>(std::lround(mono.rows * scale)));
        cv::resize(mono, processed, cv::Size(pw, ph), 0.0, 0.0, cv::INTER_AREA);
    } else {
        processed = mono;
    }

    if (scale != cached_scale_) {
        frame_K_ = cv::Mat(frame_calibration_.K).clone();
        frame_K_.at<double>(0, 0) *= scale;
        frame_K_.at<double>(1, 1) *= scale;
        frame_K_.at<double>(0, 2) *= scale;
        frame_K_.at<double>(1, 2) *= scale;
        cached_scale_ = scale;
    }

    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    detector_->detectMarkers(processed, corners, ids);

    std::map<int, bool> current;
    for (size_t i = 0; i < ids.size() && i < corners.size(); ++i) {
        const int id = ids[i];
        const auto& c = corners[i];
        if (c.size() != 4) {
            continue;
        }
        current[id] = true;

        const double area = std::fabs(cv::contourArea(c));
        MarkerPose pose;
        if (options_.detect_only) {
            pose.id = id;
            pose.area_px = area;
            pose.center_px = (c[0] + c[1] + c[2] + c[3]) * 0.25F;
            hits_[id] = 0;
        } else {
            pose = solveOne(c, id, frame_K_, frame_D_, area);
            if (pose.pose_valid) {
                const int hits = ++hits_[id];
                pose.hits = hits;
                pose.confirmed = hits >= options_.confirm_frames;
            } else {
                hits_[id] = 0;
            }
        }
        result.markers.push_back(pose);
    }

    for (auto it = hits_.begin(); it != hits_.end();) {
        if (current.count(it->first) == 0) {
            it = hits_.erase(it);
        } else {
            ++it;
        }
    }

    return result;
}
