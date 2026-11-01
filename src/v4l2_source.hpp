// SPDX-License-Identifier: MIT

#pragma once

#include "frame_source.hpp"

#include <memory>
#include <string>

// Linux V4L2 capture.  On other platforms the factory throws.
std::unique_ptr<FrameSource> makeV4L2Source(const std::string& device,
                                            int width,
                                            int height);
