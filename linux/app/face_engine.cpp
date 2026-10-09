#include "face_engine.h"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <algorithm>
#include <stdexcept>

/*
 * RFB-320 (Linzaer Ultra-Light-Fast-Generic-Face-Detector) constants.
 * Reference: Ultra-Light-Fast-Generic-Face-Detector-1MB (vision/ssd).
 */
static const int   kInputW = 320;
static const int   kInputH = 240;
static const float kDetectProbThreshold = 0.7f;
static const float kNmsThreshold = 0.3f;

static const int kNumScales = 4;

// min_boxes per scale: [[10,16,24],[32,48],[64,96],[128,192,256]]
static const int   kMinBoxesCount[kNumScales] = {3, 2, 2, 3};
static const float kMinBoxes[kNumScales][3] = {
    {10.f, 16.f, 24.f},
    {32.f, 48.f,  0.f},
    {64.f, 96.f,  0.f},
    {128.f, 192.f, 256.f},
};
static const float kStrides[kNumScales] = {8.f, 16.f, 32.f, 64.f};
static const float kCenterVariance = 0.1f;
static const float kSizeVariance   = 0.2f;

static float overlap(float x1, float y1, float w1, float h1,
                     float x2, float y2, float w2, float h2);

bool FaceEngine::init(const std::string& detect_param, const std::string& detect_bin,
                      const std::string& recog_param, const std::string& recog_bin)
{
    detector_.opt.num_threads = 1;
    recognizer_.opt.num_threads = 1;

    if (detector_.load_param(detect_param.c_str()) != 0)
        return false;
    if (detector_.load_model(detect_bin.c_str()) != 0)
        return false;
    if (recognizer_.load_param(recog_param.c_str()) != 0)
        return false;
    if (recognizer_.load_model(recog_bin.c_str()) != 0)
        return false;

    generate_priors();
    return true;
}

void FaceEngine::generate_priors()
{
    priors_.clear();
    for (int s = 0; s < kNumScales; s++) {
        int fm_w = (int)ceilf(kInputW / kStrides[s]);
        int fm_h = (int)ceilf(kInputH / kStrides[s]);
        for (int j = 0; j < fm_h; j++) {
            for (int i = 0; i < fm_w; i++) {
                float cx = (i + 0.5f) * kStrides[s] / kInputW;
                float cy = (j + 0.5f) * kStrides[s] / kInputH;
                for (int k = 0; k < kMinBoxesCount[s]; k++) {
                    float w = kMinBoxes[s][k] / kInputW;
                    float h = kMinBoxes[s][k] / kInputH;
                    priors_.push_back(cx);
                    priors_.push_back(cy);
                    priors_.push_back(w);
                    priors_.push_back(h);
                }
            }
        }
    }
    num_priors_ = (int)priors_.size() / 4;
}

std::vector<FaceBox> FaceEngine::detect(const cv::Mat& bgr)
{
    std::vector<FaceBox> faces;
    if (bgr.empty())
        return faces;

    int img_w = bgr.cols;
    int img_h = bgr.rows;

    ncnn::Mat in = ncnn::Mat::from_pixels_resize(
        bgr.data, ncnn::Mat::PIXEL_BGR2RGB, img_w, img_h, kInputW, kInputH);
    const float mean_vals[3] = {127.f, 127.f, 127.f};
    const float norm_vals[3] = {1.f / 128.f, 1.f / 128.f, 1.f / 128.f};
    in.substract_mean_normalize(mean_vals, norm_vals);

    ncnn::Extractor ex = detector_.create_extractor();
    if (ex.input("input", in) != 0) throw std::runtime_error("detector input failed");

    ncnn::Mat scores, boxes;
    if (ex.extract("scores", scores) != 0 || ex.extract("boxes", boxes) != 0 ||
        scores.empty() || boxes.empty() ||
        scores.total() != static_cast<size_t>(num_priors_) * 2 ||
        boxes.total() != static_cast<size_t>(num_priors_) * 4)
        throw std::runtime_error("detector output shape/inference failed");

    // scores: num_priors x 2 (background, face)
    // boxes : num_priors x 4 (dx, dy, dw, dh)
    const float* score_data = (const float*)scores.data;
    const float* box_data = (const float*)boxes.data;

    // decode locations to boxes
    std::vector<FaceBox> proposals;
    for (int i = 0; i < num_priors_; i++) {
        float face_score = score_data[i * 2 + 1];
        if (!std::isfinite(face_score)) continue;
        if (face_score < kDetectProbThreshold)
            continue;

        float pcx = priors_[i * 4 + 0];
        float pcy = priors_[i * 4 + 1];
        float pw  = priors_[i * 4 + 2];
        float ph  = priors_[i * 4 + 3];

        float dx = box_data[i * 4 + 0];
        float dy = box_data[i * 4 + 1];
        float dw = box_data[i * 4 + 2];
        float dh = box_data[i * 4 + 3];

        float cx = dx * kCenterVariance * pw + pcx;
        float cy = dy * kCenterVariance * ph + pcy;
        float w  = expf(dw * kSizeVariance) * pw;
        float h  = expf(dh * kSizeVariance) * ph;
        if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(w) ||
            !std::isfinite(h) || w <= 0 || h <= 0) continue;

        FaceBox fb;
        fb.x1 = (cx - w / 2.f) * img_w;
        fb.y1 = (cy - h / 2.f) * img_h;
        fb.x2 = (cx + w / 2.f) * img_w;
        fb.y2 = (cy + h / 2.f) * img_h;
        fb.score = face_score;
        proposals.push_back(fb);
    }

    // sort by score desc
    std::sort(proposals.begin(), proposals.end(),
              [](const FaceBox& a, const FaceBox& b) { return a.score > b.score; });

    // NMS
    std::vector<bool> suppressed(proposals.size(), false);
    for (size_t i = 0; i < proposals.size(); i++) {
        if (suppressed[i]) continue;
        FaceBox& a = proposals[i];
        for (size_t j = i + 1; j < proposals.size(); j++) {
            if (suppressed[j]) continue;
            FaceBox& b = proposals[j];
            float aw = a.x2 - a.x1, ah = a.y2 - a.y1;
            float bw = b.x2 - b.x1, bh = b.y2 - b.y1;
            if (overlap(a.x1, a.y1, aw, ah, b.x1, b.y1, bw, bh) > kNmsThreshold)
                suppressed[j] = true;
        }
    }

    for (size_t i = 0; i < proposals.size(); i++)
        if (!suppressed[i])
            faces.push_back(proposals[i]);

    return faces;
}

std::vector<float> FaceEngine::extract(const cv::Mat& bgr, const FaceBox& box)
{
    std::vector<float> empty;
    if (bgr.empty()) return empty;

    if (!std::isfinite(box.x1) || !std::isfinite(box.y1) ||
        !std::isfinite(box.x2) || !std::isfinite(box.y2)) return empty;
    int x1 = (int)std::max(0.f, std::min(float(bgr.cols-1), box.x1));
    int y1 = (int)std::max(0.f, std::min(float(bgr.rows-1), box.y1));
    int x2 = (int)std::max(0.f, std::min(float(bgr.cols-1), box.x2));
    int y2 = (int)std::max(0.f, std::min(float(bgr.rows-1), box.y2));
    if (x2 <= x1 || y2 <= y1) return empty;

    cv::Mat crop = bgr(cv::Rect(x1, y1, x2 - x1, y2 - y1));
    cv::Mat resized;
    cv::resize(crop, resized, cv::Size(112, 112));

    ncnn::Mat in = ncnn::Mat::from_pixels(
        resized.data, ncnn::Mat::PIXEL_BGR2RGB, 112, 112);
    const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
    const float norm_vals[3] = {1.f / 128.f, 1.f / 128.f, 1.f / 128.f};
    in.substract_mean_normalize(mean_vals, norm_vals);

    ncnn::Extractor ex = recognizer_.create_extractor();
    if (ex.input("in0", in) != 0) throw std::runtime_error("recognizer input failed");

    ncnn::Mat out;
    if (ex.extract("out0", out) != 0 || out.empty() || out.total() != 512)
        throw std::runtime_error("recognizer output shape/inference failed");

    std::vector<float> feat(out.total());
    const float* p = (const float*)out.data;
    double norm = 0;
    for (size_t i = 0; i < feat.size(); i++) {
        feat[i] = p[i];
        norm += double(p[i]) * p[i];
    }
    if (!std::isfinite(norm) || norm < 1e-12)
        throw std::runtime_error("invalid recognition embedding");

    l2_normalize(feat);
    return feat;
}

void l2_normalize(std::vector<float>& v)
{
    float norm = 0.f;
    for (float x : v) norm += x * x;
    norm = sqrtf(norm);
    if (norm < 1e-6f) return;
    for (float& x : v) x /= norm;
}

float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size()) return 0.f;
    float s = 0.f;
    for (size_t i = 0; i < a.size(); i++) s += a[i] * b[i];
    return s;
}

static float overlap(float x1, float y1, float w1, float h1,
                     float x2, float y2, float w2, float h2)
{
    float left = std::max(x1, x2);
    float top = std::max(y1, y2);
    float right = std::min(x1 + w1, x2 + w2);
    float bottom = std::min(y1 + h1, y2 + h2);
    if (right <= left || bottom <= top) return 0.f;
    float inter = (right - left) * (bottom - top);
    float uni = w1 * h1 + w2 * h2 - inter;
    return inter / uni;
}
