#include "v4l2_capture.h"
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <opencv2/imgproc.hpp>

V4L2Capture::V4L2Capture() : fd_(-1), width_(0), height_(0)
{
    memset(buffers_, 0, sizeof(buffers_));
    memset(buf_length_, 0, sizeof(buf_length_));
}

V4L2Capture::~V4L2Capture()
{
    close();
}

bool V4L2Capture::open(const std::string& device, int width, int height)
{
    close();
    const char *diagnosticEnv = std::getenv("ACCESS_CONTROL_CAPTURE_DIAG");
    diagnostics_ = diagnosticEnv && std::strcmp(diagnosticEnv, "1") == 0;
    const char *earlyEnv = std::getenv("ACCESS_CONTROL_CAPTURE_EARLY_QBUF");
    earlyQueue_ = earlyEnv && std::strcmp(earlyEnv, "1") == 0;
    frames_ = invalid_ = waits_ = dequeueErrors_ = queueErrors_ = 0;
    conversionUs_ = copyUs_ = holdUs_ = maxHoldUs_ = 0;
    diagnosticStart_ = std::chrono::steady_clock::now();
    fd_ = ::open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
        perror("open video device");
        return false;
    }

    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP");
        return false;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "not a video capture device\n");
        return false;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "device does not support streaming\n");
        return false;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT");
        return false;
    }

    width_ = fmt.fmt.pix.width;
    height_ = fmt.fmt.pix.height;
    stride_ = fmt.fmt.pix.bytesperline;
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV || width_ <= 0 ||
        height_ <= 0 || width_ % 2 || stride_ < static_cast<size_t>(width_) * 2) {
        fprintf(stderr, "unsupported camera format/stride\n");
        close();
        return false;
    }

    if (!init_mmap())
        return false;

    if (earlyQueue_) ownedYuyv_.create(height_, width_, CV_8UC2);
    fprintf(stderr, "CAPTURE_MODE v2: early_qbuf=%d (owned YUYV copy before return)\n", earlyQueue_ ? 1 : 0);

    if (diagnostics_) {
        struct v4l2_streamparm parm = {};
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        const int rc = ioctl(fd_, VIDIOC_G_PARM, &parm);
        fprintf(stderr, "CAPTURE_DIAG v2: %dx%d YUYV stride=%zu mmap_buffers=%u interval=%u/%u query_rc=%d (no frame-rate change)\n",
                width_, height_, stride_, buffer_count_,
                parm.parm.capture.timeperframe.numerator,
                parm.parm.capture.timeperframe.denominator, rc);
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        return false;
    }

    return true;
}

bool V4L2Capture::init_mmap()
{
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        return false;
    }

    if (req.count == 0 || req.count > 4) return false;
    buffer_count_ = req.count;
    for (unsigned int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            return false;
        }
        buf_length_[i] = buf.length;
        buffers_[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd_, buf.m.offset);
        if (buffers_[i] == MAP_FAILED) {
            perror("mmap");
            return false;
        }
    }

    for (unsigned int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            return false;
        }
    }

    return true;
}

bool V4L2Capture::read(cv::Mat& bgr)
{
    bgr.release();
    if (fd_ < 0) return false;
    if (diagnostics_) reportDiagnostics(false);
    struct pollfd pfd = {fd_, POLLIN, 0};
    if (::poll(&pfd, 1, 200) <= 0 || !(pfd.revents & POLLIN) ||
        (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
        if (diagnostics_) ++waits_;
        return false;
    }
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        if (diagnostics_) ++dequeueErrors_;
        return false;
    }
    const auto heldAt = diagnostics_ ? std::chrono::steady_clock::now() :
                                      std::chrono::steady_clock::time_point();
    if (buf.index >= buffer_count_) return false;
    const size_t required = stride_ * (height_ - 1) + width_ * 2;
    bool valid = !(buf.flags & V4L2_BUF_FLAG_ERROR) &&
                 buf.bytesused >= required && buf_length_[buf.index] >= required;
    bool queueAttempted = false;
    auto queuedAt = heldAt;
    const auto stamp = [this]() {
        return diagnostics_ ? std::chrono::steady_clock::now() :
                              std::chrono::steady_clock::time_point();
    };
    const auto returnBuffer = [&]() {
        queueAttempted = true;
        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
            valid = false;
            if (diagnostics_) ++queueErrors_;
        }
        queuedAt = stamp();
    };
    double copyUs = 0, convertUs = 0;
    try {
        if (valid) {
            cv::Mat yuyv(height_, width_, CV_8UC2, buffers_[buf.index], stride_);
            if (earlyQueue_) {
                const auto copyAt = stamp();
                yuyv.copyTo(ownedYuyv_);
                copyUs = std::chrono::duration<double, std::micro>(stamp() - copyAt).count();
                returnBuffer();
                // Never read the mmap-backed yuyv after QBUF, including failure paths.
                if (valid) {
                    const auto convertAt = stamp();
                    cv::cvtColor(ownedYuyv_, bgr, cv::COLOR_YUV2BGR_YUYV);
                    convertUs = std::chrono::duration<double, std::micro>(stamp() - convertAt).count();
                }
            } else {
                const auto convertAt = stamp();
                cv::cvtColor(yuyv, bgr, cv::COLOR_YUV2BGR_YUYV);
                convertUs = std::chrono::duration<double, std::micro>(stamp() - convertAt).count();
            }
        }
    } catch (...) {
        if (!queueAttempted) returnBuffer();
        throw;
    }
    if (!queueAttempted) returnBuffer();
    if (diagnostics_) {
        const double hold = std::chrono::duration<double, std::micro>(queuedAt - heldAt).count();
        ++frames_;
        if (!valid) ++invalid_;
        conversionUs_ += convertUs;
        copyUs_ += copyUs;
        holdUs_ += hold;
        if (hold > maxHoldUs_) maxHoldUs_ = hold;
    }
    if (!valid) bgr.release();
    return valid;
}

void V4L2Capture::close()
{
    if (fd_ < 0)
        return;
    if (diagnostics_) reportDiagnostics(true);

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd_, VIDIOC_STREAMOFF, &type);

    for (int i = 0; i < 4; i++) {
        if (buffers_[i] != NULL && buffers_[i] != MAP_FAILED)
            munmap(buffers_[i], buf_length_[i]);
        buffers_[i] = NULL;
    }

    ::close(fd_);
    fd_ = -1;
    buffer_count_ = 0;
    ownedYuyv_.release();
}

void V4L2Capture::reportDiagnostics(bool force)
{
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - diagnosticStart_).count();
    if (!force && seconds < 30.0) return;
    fprintf(stderr, "CAPTURE_DIAG v2: seconds=%.2f dequeued=%lu fps=%.2f invalid=%lu wait_fail=%lu dq_fail=%lu q_fail=%lu convert_avg_ms=%.3f hold_avg_ms=%.3f hold_max_ms=%.3f copy_avg_ms=%.3f early_qbuf=%d\n",
            seconds, frames_, seconds > 0 ? frames_ / seconds : 0,
            invalid_, waits_, dequeueErrors_, queueErrors_,
            frames_ ? conversionUs_ / frames_ / 1000 : 0,
            frames_ ? holdUs_ / frames_ / 1000 : 0, maxHoldUs_ / 1000,
            frames_ ? copyUs_ / frames_ / 1000 : 0, earlyQueue_ ? 1 : 0);
    diagnosticStart_ = now;
    frames_ = invalid_ = waits_ = dequeueErrors_ = queueErrors_ = 0;
    conversionUs_ = copyUs_ = holdUs_ = maxHoldUs_ = 0;
}
