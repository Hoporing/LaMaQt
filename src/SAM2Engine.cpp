#include "SAM2Engine.h"

#include <opencv2/imgproc.hpp>

#include <iostream>
#include <stdexcept>

SAM2Engine::SAM2Engine(int deviceIndex) : m_deviceIndex(deviceIndex) {}

// ─────────────────────────────────────────────────────────────────────────────
//  loadModels
// ─────────────────────────────────────────────────────────────────────────────
bool SAM2Engine::loadModels(const std::string& encoderPath,
                            const std::string& decoderPath)
{
    try {
        Ort::SessionOptions opts;
        opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);

        OrtCUDAProviderOptions cudaOpts{};
        cudaOpts.device_id = m_deviceIndex;
        opts.AppendExecutionProvider_CUDA(cudaOpts);

        std::wstring wEnc(encoderPath.begin(), encoderPath.end());
        std::wstring wDec(decoderPath.begin(), decoderPath.end());

        m_encoderSession = std::make_unique<Ort::Session>(m_env, wEnc.c_str(), opts);
        m_decoderSession = std::make_unique<Ort::Session>(m_env, wDec.c_str(), opts);

        std::cout << "[SAM2Engine] Loaded encoder: " << encoderPath << "\n"
                  << "[SAM2Engine] Loaded decoder: " << decoderPath << "\n";
        m_ready = true;
        return true;
    }
    catch (const Ort::Exception& e) {
        std::cerr << "[SAM2Engine] loadModels failed: " << e.what() << "\n";
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  encodeImage
// ─────────────────────────────────────────────────────────────────────────────
bool SAM2Engine::encodeImage(const cv::Mat& imageBGR)
{
    if (!m_ready) return false;
    try {
        m_origH = imageBGR.rows;
        m_origW = imageBGR.cols;

        // Preprocess: BGR → RGB float32 [1,3,1024,1024], ImageNet-normalized
        std::vector<float> inputData = preprocessImage(imageBGR);

        // Build input tensor (CPU → CUDA EP will move to GPU internally)
        std::vector<int64_t> inputShape = { 1, 3, INPUT_SIZE, INPUT_SIZE };
        auto inputTensor = makeCpuTensor(inputData, inputShape);

        const char* inputNames[]  = { "image" };
        const char* outputNames[] = { "high_res_feats_0", "high_res_feats_1", "image_embed" };

        auto outputs = m_encoderSession->Run(
            Ort::RunOptions{nullptr},
            inputNames,  &inputTensor, 1,
            outputNames, 3);

        // Cache encoder outputs
        // high_res_feats_0: [1, 32, 256, 256]
        auto* p0 = outputs[0].GetTensorData<float>();
        auto  s0 = outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
        m_highResFeats0.assign(p0, p0 + s0);

        // high_res_feats_1: [1, 64, 128, 128]
        auto* p1 = outputs[1].GetTensorData<float>();
        auto  s1 = outputs[1].GetTensorTypeAndShapeInfo().GetElementCount();
        m_highResFeats1.assign(p1, p1 + s1);

        // image_embed: [1, 256, 64, 64]
        auto* pe = outputs[2].GetTensorData<float>();
        auto  se = outputs[2].GetTensorTypeAndShapeInfo().GetElementCount();
        m_imageEmbed.assign(pe, pe + se);

        return true;
    }
    catch (const Ort::Exception& e) {
        std::cerr << "[SAM2Engine] encodeImage failed: " << e.what() << "\n";
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  decode
// ─────────────────────────────────────────────────────────────────────────────
bool SAM2Engine::decode(const std::vector<cv::Point2f>& points,
                        const std::vector<int>&         labels,
                        cv::Mat&                        outMask)
{
    if (!m_ready || m_imageEmbed.empty()) return false;
    if (points.empty() || points.size() != labels.size()) return false;

    try {
        const int N = static_cast<int>(points.size());

        // ── 1. Build decoder inputs ──────────────────────────────────────────

        // point_coords [1, N, 2]: pixel coords → 1024-space
        std::vector<float> coords(N * 2);
        for (int i = 0; i < N; ++i) {
            coords[i * 2 + 0] = points[i].x * static_cast<float>(INPUT_SIZE) / m_origW;
            coords[i * 2 + 1] = points[i].y * static_cast<float>(INPUT_SIZE) / m_origH;
        }

        // point_labels [1, N]
        std::vector<float> lbls(N);
        for (int i = 0; i < N; ++i)
            lbls[i] = static_cast<float>(labels[i]);

        // mask_input [1, 1, 256, 256] — zeros (no previous mask)
        std::vector<float> maskInput(1 * 1 * 256 * 256, 0.0f);

        // has_mask_input [1] — 0 (no previous mask)
        std::vector<float> hasMask(1, 0.0f);

        // ── 2. Build Ort::Value tensors ──────────────────────────────────────

        auto tEmbed  = makeCpuTensor(m_imageEmbed,    { 1, 256, 64,  64  });
        auto tHigh0  = makeCpuTensor(m_highResFeats0, { 1, 32,  256, 256 });
        auto tHigh1  = makeCpuTensor(m_highResFeats1, { 1, 64,  128, 128 });
        auto tCoords = makeCpuTensor(coords,          { 1, N,   2        });
        auto tLabels = makeCpuTensor(lbls,            { 1, N             });
        auto tMaskIn = makeCpuTensor(maskInput,       { 1, 1,   256, 256 });
        auto tHasMsk = makeCpuTensor(hasMask,         { 1                });

        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(std::move(tEmbed));
        inputTensors.push_back(std::move(tHigh0));
        inputTensors.push_back(std::move(tHigh1));
        inputTensors.push_back(std::move(tCoords));
        inputTensors.push_back(std::move(tLabels));
        inputTensors.push_back(std::move(tMaskIn));
        inputTensors.push_back(std::move(tHasMsk));

        const char* inputNames[] = {
            "image_embed", "high_res_feats_0", "high_res_feats_1",
            "point_coords", "point_labels", "mask_input", "has_mask_input"
        };
        const char* outputNames[] = { "masks", "iou_predictions" };

        auto outputs = m_decoderSession->Run(
            Ort::RunOptions{nullptr},
            inputNames,  inputTensors.data(), inputTensors.size(),
            outputNames, 2);

        // ── 3. Pick best mask (highest IoU) ─────────────────────────────────

        // iou_predictions: [1, 3]
        const float* iouPtr  = outputs[1].GetTensorData<float>();
        int bestIdx = 0;
        for (int i = 1; i < 3; ++i)
            if (iouPtr[i] > iouPtr[bestIdx]) bestIdx = i;

        // masks: [1, 3, 256, 256]
        const float* maskPtr = outputs[0].GetTensorData<float>();
        const int    planeSz = 256 * 256;

        cv::Mat lowResMask(256, 256, CV_32FC1,
            const_cast<float*>(maskPtr + bestIdx * planeSz));

        // ── 4. Upsample [256,256] → original resolution + threshold ─────────

        cv::Mat upsampled;
        cv::resize(lowResMask, upsampled, cv::Size(m_origW, m_origH),
                   0, 0, cv::INTER_LINEAR);

        // threshold > 0.0 → binary mask
        outMask = cv::Mat(m_origH, m_origW, CV_8UC1);
        for (int r = 0; r < m_origH; ++r)
            for (int c = 0; c < m_origW; ++c)
                outMask.at<uchar>(r, c) = (upsampled.at<float>(r, c) > 0.0f) ? 255u : 0u;

        return true;
    }
    catch (const Ort::Exception& e) {
        std::cerr << "[SAM2Engine] decode failed: " << e.what() << "\n";
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  preprocessImage
// ─────────────────────────────────────────────────────────────────────────────
std::vector<float> SAM2Engine::preprocessImage(const cv::Mat& imageBGR)
{
    // 1. Resize to 1024x1024
    cv::Mat resized;
    cv::resize(imageBGR, resized, cv::Size(INPUT_SIZE, INPUT_SIZE),
               0, 0, cv::INTER_LINEAR);

    // 2. BGR → RGB, uint8 → float32 [0,1]
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    cv::Mat rgbf;
    rgb.convertTo(rgbf, CV_32FC3, 1.0 / 255.0);

    // 3. NHWC → NCHW with ImageNet normalization
    std::vector<float> data(1 * 3 * INPUT_SIZE * INPUT_SIZE);
    const int planeSize = INPUT_SIZE * INPUT_SIZE;

    for (int r = 0; r < INPUT_SIZE; ++r) {
        const float* row = rgbf.ptr<float>(r);
        for (int c = 0; c < INPUT_SIZE; ++c) {
            float pixR = row[c * 3 + 0];
            float pixG = row[c * 3 + 1];
            float pixB = row[c * 3 + 2];
            data[0 * planeSize + r * INPUT_SIZE + c] = (pixR - MEAN[0]) / STD[0];
            data[1 * planeSize + r * INPUT_SIZE + c] = (pixG - MEAN[1]) / STD[1];
            data[2 * planeSize + r * INPUT_SIZE + c] = (pixB - MEAN[2]) / STD[2];
        }
    }
    return data;
}

// ─────────────────────────────────────────────────────────────────────────────
//  makeCpuTensor
// ─────────────────────────────────────────────────────────────────────────────
Ort::Value SAM2Engine::makeCpuTensor(const std::vector<float>& data,
                                     const std::vector<int64_t>& shape)
{
    auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    return Ort::Value::CreateTensor<float>(
        memInfo,
        const_cast<float*>(data.data()), data.size(),
        shape.data(), shape.size());
}
