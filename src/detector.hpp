// SPDX-License-Identifier: MIT

#pragma once

#include "calibration.hpp"

#include <opencv2/core.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

struct DetectorOptions {
    int process_width = 640;
    int confirm_frames = 3;       // consecutive valid poses before confirmed
    bool detect_only = false;     // skip PnP, report pixel detections
    bool use_aruco3 = false;      // opt-in: faster on large images
    std::string corner_refine = "none";
};

// One marker observation.  Pose fields are in the robot body frame
// (x forward, y left, z up).  bearing_rad is the body-frame polar angle,
// positive to the left (CCW); output.cpp flips it to the compass-style
// positive-right bearing used by the CLI.
struct MarkerPose {
    int id = -1;
    bool pose_valid = false;
    bool confirmed = false;
    int hits = 0;

    double bearing_rad = 0.0;       // + left (body frame)
    double planar_range_m = 0.0;    // horizontal distance
    double up_m = 0.0;              // camera-relative vertical offset

    double rms_px = 0.0;
    double area_px = 0.0;
    cv::Point2f center_px{0.0F, 0.0F};
};

struct FrameResult {
    std::vector<MarkerPose> markers;  // detections in this frame (valid or not)
};

class MarkerDetector {
public:
    MarkerDetector(const Calibration& calibration, const DetectorOptions& options);

    FrameResult detect(const cv::Mat& gray);

    const Calibration& calibration() const { return calibration_; }
    const DetectorOptions& options() const { return options_; }

private:
    MarkerPose solveOne(const std::vector<cv::Point2f>& corners,
                        int id,
                        const cv::Mat& K,
                        const cv::Mat& D,
                        double area_px);

    Calibration calibration_;
    DetectorOptions options_;
    cv::aruco::Dictionary dictionary_;
    std::unique_ptr<cv::aruco::ArucoDetector> detector_;

    Calibration frame_calibration_;
    int calibrated_for_width_ = 0;
    int calibrated_for_height_ = 0;

    // Reused per frame/marker.  K is rebuilt only when the source resolution
    // or processing scale changes; D is rebuilt only when calibration changes.
    cv::Mat frame_K_;
    cv::Mat frame_D_;
    double cached_scale_ = -1.0;

    // Constant for one camera/marker combination.
    std::vector<cv::Point3f> object_points_;
    cv::Mat identity_K_;
    cv::Mat zero_dist_;

    // Consecutive valid frames per marker.  Missing or invalid frames reset it.
    std::map<int, int> hits_;
};
