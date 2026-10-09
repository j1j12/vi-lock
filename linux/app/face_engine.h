#ifndef FACE_ENGINE_H
#define FACE_ENGINE_H

#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "net.h"

/*
 * Face detection + recognition engine.
 *
 * Detector : RFB-320 (Linzaer Ultra-Light-Fast family), input 320x240,
 *            4420 anchors, mean=127/std=128.
 * Recognizer: MobileFaceNet (w600k_mbf), input 112x112, 512-dim embedding.
 */

struct FaceBox {
    float x1, y1, x2, y2;   // pixel coords in the original frame
    float score;
};

class FaceEngine {
public:
    bool init(const std::string& detect_param, const std::string& detect_bin,
              const std::string& recog_param, const std::string& recog_bin);

    // Detect faces in a BGR image, returns face boxes (pixel coords).
    std::vector<FaceBox> detect(const cv::Mat& bgr);

    // Extract the 512-dim embedding for a face box.
    std::vector<float> extract(const cv::Mat& bgr, const FaceBox& box);

private:
    void generate_priors();

    ncnn::Net detector_;
    ncnn::Net recognizer_;

    int num_priors_;
    std::vector<float> priors_;   // [cx, cy, w, h] x num_priors (normalized)
};

// L2-normalize a feature vector in place.
void l2_normalize(std::vector<float>& v);

// Cosine similarity between two normalized feature vectors.
float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);

#endif // FACE_ENGINE_H
