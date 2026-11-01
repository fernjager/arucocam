// SPDX-License-Identifier: MIT

// Test helper: render one ArUco marker into a PGM canvas.
//
// This is built only with -DARUCOCAM_BUILD_TESTS=ON.  It is intentionally not
// part of the deployed arucocam binary.
#include <opencv2/objdetect/aruco_detector.hpp>
#include <opencv2/objdetect/aruco_dictionary.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string output = argc > 1 ? argv[1] : "tests/marker.pgm";
    const int id = argc > 2 ? std::atoi(argv[2]) : 7;
    const int side = argc > 3 ? std::atoi(argv[3]) : 240;
    const int center_x = argc > 4 ? std::atoi(argv[4]) : 320;
    const int center_y = argc > 5 ? std::atoi(argv[5]) : 240;

    cv::Mat marker;
    const auto dictionary =
        cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
    cv::aruco::generateImageMarker(dictionary, id, side, marker, 1);

    cv::Mat canvas(480, 640, CV_8UC1, cv::Scalar(255));
    const int x = center_x - side / 2;
    const int y = center_y - side / 2;
    if (x < 0 || y < 0 || x + side > canvas.cols || y + side > canvas.rows) {
        std::cerr << "marker does not fit on the canvas\n";
        return 1;
    }
    marker.copyTo(canvas(cv::Rect(x, y, side, side)));

    if (!cv::imwrite(output, canvas)) {
        std::cerr << "could not write " << output << '\n';
        return 1;
    }
    std::cout << "wrote " << output << " id=" << id << " side=" << side
              << " center=" << center_x << "," << center_y << '\n';
    return 0;
}
