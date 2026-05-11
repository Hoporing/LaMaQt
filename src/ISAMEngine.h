#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

// ============================================================================
//  ISAMEngine  -  Abstract interface for SAM-based segmentation engines
//
//  Designed for easy model swap:
//    SAM2Engine : public ISAMEngine   (point prompt, current)
//    SAM3Engine : public ISAMEngine   (point + text prompt, future)
//
//  Typical usage:
//    engine->loadModels(encoderPath, decoderPath);
//    engine->encodeImage(imageBGR);          // once per image
//    engine->decode(points, labels, mask);   // once per click
// ============================================================================

class ISAMEngine {
public:
    virtual ~ISAMEngine() = default;

    // Load ONNX models.
    // encoderPath : image encoder ONNX
    // decoderPath : mask decoder ONNX
    virtual bool loadModels(const std::string& encoderPath,
                            const std::string& decoderPath) = 0;

    // Encode the source image.  Must be called whenever the image changes.
    // imageBGR : CV_8UC3 BGR image (any resolution)
    virtual bool encodeImage(const cv::Mat& imageBGR) = 0;

    // Decode a mask from accumulated point prompts.
    // points : image-space pixel coordinates (not 1024-space)
    // labels : 1 = foreground, 0 = background
    // outMask: CV_8UC1 binary mask, same size as the image passed to encodeImage()
    virtual bool decode(const std::vector<cv::Point2f>& points,
                        const std::vector<int>&         labels,
                        cv::Mat&                        outMask) = 0;

    virtual bool isReady() const = 0;
};
