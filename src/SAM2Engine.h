#pragma once

#include "ISAMEngine.h"

#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>

#include <memory>
#include <string>
#include <vector>

// ============================================================================
//  SAM2Engine  -  SAM 2 inference via ONNX Runtime (CUDA EP)
//
//  Model files (samexporter format):
//    encoder: sam2_hiera_*.encoder.onnx
//    decoder: sam2_hiera_*.decoder.onnx
//
//  Pipeline:
//    1. loadModels()     : load encoder + decoder ONNX sessions
//    2. encodeImage()    : BGR → [1,3,1024,1024] → encoder
//                          stores high_res_feats_0/1 + image_embed
//    3. decode()         : (embedding + points) → decoder
//                          picks best of 3 candidate masks (highest IoU)
//                          upsamples [256,256] → original resolution
//
//  All OnnxRuntime sessions run on the CUDA EP.
//  Image preprocessing (resize + normalize) runs on CPU via OpenCV.
// ============================================================================

class SAM2Engine : public ISAMEngine {
public:
    explicit SAM2Engine(int deviceIndex = 0);
    ~SAM2Engine() override = default;

    bool loadModels(const std::string& encoderPath,
                    const std::string& decoderPath) override;

    bool encodeImage(const cv::Mat& imageBGR) override;

    bool decode(const std::vector<cv::Point2f>& points,
                const std::vector<int>&         labels,
                cv::Mat&                        outMask) override;

    bool isReady() const override { return m_ready; }

private:
    // Preprocess: BGR → RGB float32 [1,3,1024,1024], ImageNet-normalized
    std::vector<float> preprocessImage(const cv::Mat& imageBGR);

    // Create a CPU Ort::Value from a float vector + shape
    Ort::Value makeCpuTensor(const std::vector<float>& data,
                             const std::vector<int64_t>& shape);

    int  m_deviceIndex = 0;
    bool m_ready       = false;

    Ort::Env m_env{ ORT_LOGGING_LEVEL_WARNING, "SAM2Engine" };

    std::unique_ptr<Ort::Session> m_encoderSession;
    std::unique_ptr<Ort::Session> m_decoderSession;

    // Cached encoder outputs (reused across decode() calls for the same image)
    std::vector<float> m_imageEmbed;      // [1, 256, 64, 64]
    std::vector<float> m_highResFeats0;   // [1, 32,  256, 256]
    std::vector<float> m_highResFeats1;   // [1, 64,  128, 128]

    // Original image size (set in encodeImage, used in decode for upsampling)
    int m_origH = 0;
    int m_origW = 0;

    // ImageNet normalization constants
    static constexpr float MEAN[3] = { 0.485f, 0.456f, 0.406f };
    static constexpr float STD[3]  = { 0.229f, 0.224f, 0.225f };
    static constexpr int   INPUT_SIZE = 1024;
};
