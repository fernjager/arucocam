// SPDX-License-Identifier: MIT

#include "v4l2_source.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef __linux__

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

namespace {

int xioctl(int fd, unsigned long request, void* arg) {
    int r = 0;
    do {
        r = ioctl(fd, request, arg);
    } while (r < 0 && errno == EINTR);
    return r;
}

[[noreturn]] void failErrno(const std::string& what) {
    throw std::runtime_error(what + ": " + std::strerror(errno));
}

std::string fourcc(uint32_t format) {
    char s[5];
    s[0] = static_cast<char>(format & 0xFF);
    s[1] = static_cast<char>((format >> 8) & 0xFF);
    s[2] = static_cast<char>((format >> 16) & 0xFF);
    s[3] = static_cast<char>((format >> 24) & 0xFF);
    s[4] = '\0';
    return std::string(s);
}

bool supportedFormat(uint32_t format) {
    switch (format) {
        case V4L2_PIX_FMT_GREY:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_MJPEG:
        case V4L2_PIX_FMT_JPEG:
        case V4L2_PIX_FMT_RGB24:
        case V4L2_PIX_FMT_BGR24:
            return true;
        default:
            return false;
    }
}

}  // namespace

class V4L2Source : public FrameSource {
public:
    V4L2Source(std::string device, int width, int height)
        : device_(std::move(device)) {
        try {
            openDevice();
            setFormat(width, height);
            startStreaming();
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~V4L2Source() override { cleanup(); }

    bool next(Frame& frame) override {
        if (fd_ < 0) {
            return false;
        }

        v4l2_buffer newest{};
        int newest_index = -1;

        // Drain every buffer that is already ready.  The camera keeps
        // producing while the Pi is doing ArUco, so the oldest queued frame is
        // stale and must not be used for control.  Holding the newest while
        // requeueing the older ones leaves exactly one buffer in flight.
        for (;;) {
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (xioctl(fd_, VIDIOC_DQBUF, &buf) != 0) {
                if (errno == EAGAIN) {
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                failErrno("VIDIOC_DQBUF");
            }
            if (buf.index >= buffers_.size()) {
                throw std::runtime_error("V4L2 returned an invalid buffer index");
            }
            if (newest_index >= 0) {
                queue(newest_index);
            }
            newest = buf;
            newest_index = static_cast<int>(buf.index);
        }

        if (newest_index < 0) {
            if (!waitForFrame()) {
                return false;
            }
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (xioctl(fd_, VIDIOC_DQBUF, &buf) != 0) {
                if (errno == EINTR || errno == EAGAIN) {
                    return false;
                }
                failErrno("VIDIOC_DQBUF");
            }
            if (buf.index >= buffers_.size()) {
                throw std::runtime_error("V4L2 returned an invalid buffer index");
            }
            newest = buf;
            newest_index = static_cast<int>(buf.index);
        }

        if ((newest.flags & V4L2_BUF_FLAG_ERROR) != 0) {
            queue(newest_index);
            return false;
        }

        cv::Mat image;
        convert(newest, image);
        queue(newest_index);

        frame.gray = std::move(image);
        return true;
    }

    bool isLive() const override { return true; }

private:
    struct Buffer {
        void* start = nullptr;
        size_t length = 0;
    };

    void openDevice() {
        fd_ = open(device_.c_str(), O_RDWR | O_NONBLOCK);
        if (fd_ < 0) {
            failErrno("cannot open " + device_);
        }

        v4l2_capability cap{};
        if (xioctl(fd_, VIDIOC_QUERYCAP, &cap) != 0) {
            failErrno("VIDIOC_QUERYCAP " + device_);
        }
        const uint32_t caps =
            (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps
                                                       : cap.capabilities;
        if ((caps & V4L2_CAP_VIDEO_CAPTURE) == 0) {
            throw std::runtime_error(device_ + " is not a video capture device");
        }
        if ((caps & V4L2_CAP_STREAMING) == 0) {
            throw std::runtime_error(device_ + " does not support streaming I/O");
        }
    }

    void setFormat(int width, int height) {
        if (width <= 0 || height <= 0) {
            throw std::runtime_error("V4L2 capture needs positive width and height");
        }

        // Prefer formats whose luma plane can be used directly.  YUV420/NV12
        // are a straight copy of Y; YUYV/UYVY still need a color conversion;
        // MJPEG is the fallback because every frame costs a JPEG decode.
        const std::vector<uint32_t> formats = {
            V4L2_PIX_FMT_GREY,
            V4L2_PIX_FMT_YUV420,
            V4L2_PIX_FMT_NV12,
            V4L2_PIX_FMT_YUYV,
            V4L2_PIX_FMT_UYVY,
            V4L2_PIX_FMT_MJPEG,
            V4L2_PIX_FMT_JPEG,
            V4L2_PIX_FMT_RGB24,
            V4L2_PIX_FMT_BGR24,
        };

        std::string last;
        for (uint32_t format : formats) {
            v4l2_format fmt{};
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            fmt.fmt.pix.width = static_cast<uint32_t>(width);
            fmt.fmt.pix.height = static_cast<uint32_t>(height);
            fmt.fmt.pix.pixelformat = format;
            fmt.fmt.pix.field = V4L2_FIELD_ANY;
            if (xioctl(fd_, VIDIOC_S_FMT, &fmt) != 0) {
                continue;
            }
            last = fourcc(fmt.fmt.pix.pixelformat) + " " +
                   std::to_string(fmt.fmt.pix.width) + "x" +
                   std::to_string(fmt.fmt.pix.height);
            if (static_cast<int>(fmt.fmt.pix.width) != width ||
                static_cast<int>(fmt.fmt.pix.height) != height ||
                !supportedFormat(fmt.fmt.pix.pixelformat)) {
                continue;
            }
            pixel_format_ = fmt.fmt.pix.pixelformat;
            width_ = static_cast<int>(fmt.fmt.pix.width);
            height_ = static_cast<int>(fmt.fmt.pix.height);
            bytes_per_line_ = static_cast<size_t>(fmt.fmt.pix.bytesperline);
            if (bytes_per_line_ == 0) {
                switch (pixel_format_) {
                    case V4L2_PIX_FMT_YUYV:
                    case V4L2_PIX_FMT_UYVY:
                        bytes_per_line_ = static_cast<size_t>(width_) * 2;
                        break;
                    case V4L2_PIX_FMT_RGB24:
                    case V4L2_PIX_FMT_BGR24:
                        bytes_per_line_ = static_cast<size_t>(width_) * 3;
                        break;
                    default:
                        bytes_per_line_ = static_cast<size_t>(width_);
                        break;
                }
            }
            return;
        }

        throw std::runtime_error(
            "no supported V4L2 format at " + std::to_string(width) + "x" +
            std::to_string(height) + " on " + device_ +
            (last.empty() ? "" : " (driver offered " + last + ")"));
    }

    void startStreaming() {
        v4l2_requestbuffers req{};
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd_, VIDIOC_REQBUFS, &req) != 0) {
            failErrno("VIDIOC_REQBUFS");
        }
        if (req.count < 2) {
            throw std::runtime_error("V4L2 driver provided fewer than 2 buffers");
        }

        buffers_.resize(req.count);
        for (uint32_t i = 0; i < req.count; ++i) {
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (xioctl(fd_, VIDIOC_QUERYBUF, &buf) != 0) {
                failErrno("VIDIOC_QUERYBUF");
            }
            buffers_[i].length = buf.length;
            buffers_[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd_, buf.m.offset);
            if (buffers_[i].start == MAP_FAILED) {
                buffers_[i].start = nullptr;
                failErrno("mmap V4L2 buffer");
            }
        }

        for (uint32_t i = 0; i < req.count; ++i) {
            queue(static_cast<int>(i));
        }

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd_, VIDIOC_STREAMON, &type) != 0) {
            failErrno("VIDIOC_STREAMON");
        }
        streaming_ = true;
    }

    void queue(int index) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = static_cast<uint32_t>(index);
        if (xioctl(fd_, VIDIOC_QBUF, &buf) != 0) {
            failErrno("VIDIOC_QBUF");
        }
    }

    bool waitForFrame() {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);
        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        const int r = select(fd_ + 1, &fds, nullptr, nullptr, &timeout);
        if (r < 0) {
            if (errno == EINTR) {
                return false;
            }
            failErrno("select V4L2");
        }
        return r > 0;
    }

    void convert(const v4l2_buffer& buf, cv::Mat& out) const {
        const auto* data =
            static_cast<const unsigned char*>(buffers_[buf.index].start);
        const size_t step = bytes_per_line_;

        switch (pixel_format_) {
            case V4L2_PIX_FMT_GREY: {
                cv::Mat view(height_, width_, CV_8UC1,
                             const_cast<unsigned char*>(data), step);
                out = view.clone();
                break;
            }
            case V4L2_PIX_FMT_YUYV: {
                cv::Mat view(height_, width_, CV_8UC2,
                             const_cast<unsigned char*>(data), step);
                cv::cvtColor(view, out, cv::COLOR_YUV2GRAY_YUYV);
                break;
            }
            case V4L2_PIX_FMT_UYVY: {
                cv::Mat view(height_, width_, CV_8UC2,
                             const_cast<unsigned char*>(data), step);
                cv::cvtColor(view, out, cv::COLOR_YUV2GRAY_UYVY);
                break;
            }
            case V4L2_PIX_FMT_YUV420: {
                cv::Mat view(height_, width_, CV_8UC1,
                             const_cast<unsigned char*>(data), step);
                out = view.clone();
                break;
            }
            case V4L2_PIX_FMT_NV12: {
                cv::Mat view(height_, width_, CV_8UC1,
                             const_cast<unsigned char*>(data), step);
                out = view.clone();
                break;
            }
            case V4L2_PIX_FMT_MJPEG:
            case V4L2_PIX_FMT_JPEG: {
                if (buf.bytesused == 0) {
                    throw std::runtime_error("empty MJPEG buffer");
                }
                cv::Mat encoded(1, static_cast<int>(buf.bytesused), CV_8UC1,
                                const_cast<unsigned char*>(data));
                out = cv::imdecode(encoded, cv::IMREAD_GRAYSCALE);
                break;
            }
            case V4L2_PIX_FMT_RGB24: {
                cv::Mat view(height_, width_, CV_8UC3,
                             const_cast<unsigned char*>(data), step);
                cv::cvtColor(view, out, cv::COLOR_RGB2GRAY);
                break;
            }
            case V4L2_PIX_FMT_BGR24: {
                cv::Mat view(height_, width_, CV_8UC3,
                             const_cast<unsigned char*>(data), step);
                cv::cvtColor(view, out, cv::COLOR_BGR2GRAY);
                break;
            }
            default:
                throw std::runtime_error("unsupported V4L2 pixel format");
        }

        if (out.empty()) {
            throw std::runtime_error("V4L2 frame decode failed");
        }
    }

    void cleanup() {
        if (fd_ >= 0 && streaming_) {
            v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            xioctl(fd_, VIDIOC_STREAMOFF, &type);
            streaming_ = false;
        }
        for (auto& b : buffers_) {
            if (b.start != nullptr && b.start != MAP_FAILED) {
                munmap(b.start, b.length);
            }
            b.start = nullptr;
        }
        buffers_.clear();
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    std::string device_;
    int fd_ = -1;
    bool streaming_ = false;
    uint32_t pixel_format_ = 0;
    int width_ = 0;
    int height_ = 0;
    size_t bytes_per_line_ = 0;
    std::vector<Buffer> buffers_;
};

std::unique_ptr<FrameSource> makeV4L2Source(const std::string& device,
                                            int width,
                                            int height) {
    return std::make_unique<V4L2Source>(device, width, height);
}

#else

std::unique_ptr<FrameSource> makeV4L2Source(const std::string&,
                                            int,
                                            int) {
    throw std::runtime_error("V4L2 capture is only available on Linux");
}

#endif
