#ifndef V4L2_CAPTURE_H
#define V4L2_CAPTURE_H

#include <string>
#include <chrono>
#include <opencv2/core.hpp>

/*
 * Minimal V4L2 capture wrapper for the OV5640 camera (DCMI).
 * Captures YUYV frames and converts to BGR (cv::Mat).
 */
class V4L2Capture {
public:
    V4L2Capture();
    ~V4L2Capture();

    bool open(const std::string& device, int width, int height);
    void close();

    int width() const  { return width_; }
    int height() const { return height_; }

    // Capture one frame and convert to BGR. Returns false on error.
    bool read(cv::Mat& bgr);

private:
    bool init_mmap();
    void reportDiagnostics(bool force);
    bool diagnostics_ = false;
    bool earlyQueue_ = false;
    cv::Mat ownedYuyv_;
    std::chrono::steady_clock::time_point diagnosticStart_;
    unsigned long frames_ = 0, invalid_ = 0, waits_ = 0, dequeueErrors_ = 0, queueErrors_ = 0;
    double conversionUs_ = 0, copyUs_ = 0, holdUs_ = 0, maxHoldUs_ = 0;

    int fd_;
    int width_;
    int height_;
    size_t stride_ = 0;
    unsigned int buffer_count_ = 0;
    void* buffers_[4];
    size_t buf_length_[4];
};

#endif // V4L2_CAPTURE_H
