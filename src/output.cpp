// SPDX-License-Identifier: MIT

#include "output.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string fixed(double value, int precision) {
    if (!std::isfinite(value)) {
        return "null";
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string signedFixed(double value, int precision) {
    if (!std::isfinite(value)) {
        return "null";
    }
    std::ostringstream oss;
    oss << std::showpos << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

double degrees(double radians) {
    return radians * 180.0 / kPi;
}

// MarkerPose stores the body-frame angle, positive left (CCW).  The CLI
// reports a compass-style bearing, positive right (CW), so flip the sign at
// this boundary.  Normalising +0 also avoids printing "-0.0" dead ahead.
double reportedBearingDeg(const MarkerPose& m) {
    const double value = -degrees(m.bearing_rad);
    return value == 0.0 ? 0.0 : value;
}

bool visible(const FrameReport& r, const MarkerPose& m) {
    return r.target_filter < 0 || m.id == r.target_filter;
}

}  // namespace

void printSimpleJson(const FrameReport& r) {
    std::map<int, const MarkerPose*> markers;
    for (const auto& m : r.markers) {
        if (!m.pose_valid || !m.confirmed || !visible(r, m)) {
            continue;
        }
        markers[m.id] = &m;
    }

    std::ostringstream out;
    out << '{';
    bool first = true;
    for (const auto& entry : markers) {
        if (!first) {
            out << ',';
        }
        first = false;
        const MarkerPose& m = *entry.second;
        out << '"' << entry.first << "\":{\"bearing\":"
            << fixed(reportedBearingDeg(m), 1) << ",\"horiz\":"
            << fixed(m.planar_range_m, 2) << ",\"vert\":"
            << fixed(m.up_m, 2) << '}';
    }
    if (r.timing) {
        if (!first) {
            out << ',';
        }
        out << "\"_timing\":{\"processMs\":" << fixed(r.process_ms, 3) << '}';
    }
    out << '}';
    std::cout << out.str() << '\n';
}

void printHuman(const FrameReport& r) {
    bool any = false;
    for (const auto& m : r.markers) {
        if (!visible(r, m)) {
            continue;
        }
        any = true;
        if (!m.pose_valid) {
            std::cout << "id=" << m.id << " detected center=("
                      << fixed(m.center_px.x, 1) << ',' << fixed(m.center_px.y, 1)
                      << ") area=" << fixed(m.area_px, 0) << "px";
            if (!r.detect_only) {
                std::cout << " rejected rms=" << fixed(m.rms_px, 2) << "px";
            }
            std::cout << '\n';
            continue;
        }

        std::cout << "id=" << m.id
                  << " bearing=" << signedFixed(reportedBearingDeg(m), 1) << "deg"
                  << " horiz=" << fixed(m.planar_range_m, 2) << "m"
                  << " vert=" << signedFixed(m.up_m, 2) << "m";
        if (r.verbose) {
            std::cout << " rms=" << fixed(m.rms_px, 2) << "px"
                      << " area=" << fixed(m.area_px, 0) << "px";
        }
        std::cout << '\n';
    }
    if (!any) {
        std::cout << "no markers\n";
    }
}
