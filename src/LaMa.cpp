#include "LaMa.h"

#include <opencv2/cudaimgproc.hpp>

// ============================================================================
//  LaMa  -  Implementation
// ============================================================================

LaMa::LaMa(const std::string& onnxModelPath)
{
    OnnxEngine::Options options;
    options.deviceIndex = 0;

    m_onnxEngine = std::make_unique<OnnxEngine>(options);

    try {
        if (!m_onnxEngine->load(onnxModelPath))
            throw std::runtime_error(
                "[LaMa] ONNX Runtime model load failed.\n"
                "  - Verify lama.onnx path: " + onnxModelPath
            );

        // Cache GPU buffer pointers (device memory owned by OnnxEngine)
        m_inputBuffer  = m_onnxEngine->getInputBuffer();
        m_outputBuffer = m_onnxEngine->getOutputBuffer();

        if (!m_inputBuffer || !m_outputBuffer)
            throw std::runtime_error("[LaMa] I/O buffer initialization failed.");

        std::cout << "[LaMa] Engine ready  (ONNX Runtime, CUDA EP, FP32)" << std::endl;
        m_bReady = true;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  inpaint
// ─────────────────────────────────────────────────────────────────────────────
cv::Mat LaMa::inpaint(const cv::Mat& imageBGR, const cv::Mat& mask)
{
    if (!m_bReady) {
        std::cerr << "[LaMa] Engine not initialized." << std::endl;
        return {};
    }
    if (imageBGR.empty() || mask.empty()) {
        std::cerr << "[LaMa] Empty input." << std::endl;
        return {};
    }
    if (imageBGR.size() != mask.size()) {
        std::cerr << "[LaMa] Image/mask size mismatch  "
                  << imageBGR.size() << " vs " << mask.size() << std::endl;
        return {};
    }
    // No hard size limit — any resolution is resized to EXPORT_H x EXPORT_W for inference.

    try {
        preprocess(imageBGR, mask);
        m_onnxEngine->runInference();
        return postprocess();
    }
    catch (const std::exception& e) {
        std::cerr << "[LaMa] Inference error: " << e.what() << std::endl;
        return {};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  preprocess  -  fully on GPU
// ─────────────────────────────────────────────────────────────────────────────
void LaMa::preprocess(const cv::Mat& imageBGR, const cv::Mat& maskInput)
{
    m_origH  = imageBGR.rows;
    m_origW  = imageBGR.cols;
    m_inferH = EXPORT_H;
    m_inferW = EXPORT_W;

    const int planeSize = EXPORT_H * EXPORT_W;

    // ── 1. Upload to GPU ──────────────────────────────────────────────────────
    cv::cuda::GpuMat gpuBGR, gpuMaskRaw;
    gpuBGR.upload(imageBGR);

    // Ensure single-channel mask
    if (maskInput.channels() == 3) {
        cv::cuda::GpuMat gpuMask3;
        gpuMask3.upload(maskInput);
        cv::cuda::cvtColor(gpuMask3, gpuMaskRaw, cv::COLOR_BGR2GRAY);
    } else {
        gpuMaskRaw.upload(maskInput);
    }

    // ── 2. BGR -> RGB,  [0,255] -> [0,1] ─────────────────────────────────────
    cv::cuda::GpuMat gpuRGB, gpuRGBf;
    cv::cuda::cvtColor(gpuBGR, gpuRGB, cv::COLOR_BGR2RGB);
    gpuRGB.convertTo(gpuRGBf, CV_32FC3, 1.0 / 255.0);

    // ── 3. Mask binarization: > 0 -> 1.0 ─────────────────────────────────────
    cv::cuda::GpuMat gpuMaskf;
    gpuMaskRaw.convertTo(gpuMaskf, CV_32FC1, 1.0 / 255.0);
    cv::cuda::threshold(gpuMaskf, gpuMaskf, 0.0, 1.0, cv::THRESH_BINARY);

    // ── 4. Resize to fixed inference resolution (EXPORT_H x EXPORT_W) ────────
    //  Image : bilinear (quality)
    //  Mask  : nearest-neighbor + re-binarize (preserve hard mask edges)
    cv::cuda::resize(gpuRGBf,  m_gpuOrigPadRGB, cv::Size(EXPORT_W, EXPORT_H), 0, 0, cv::INTER_LINEAR);
    cv::cuda::resize(gpuMaskf, m_gpuMaskPad,    cv::Size(EXPORT_W, EXPORT_H), 0, 0, cv::INTER_NEAREST);
    cv::cuda::threshold(m_gpuMaskPad, m_gpuMaskPad, 0.5, 1.0, cv::THRESH_BINARY);

    // ── 5. masked_image = rgb * (1 - mask),  per-channel ─────────────────────
    cv::cuda::GpuMat invMask;
    cv::cuda::addWeighted(m_gpuMaskPad, -1.0, m_gpuMaskPad, 0.0, 1.0, invMask);

    std::vector<cv::cuda::GpuMat> rgbCh(3);
    cv::cuda::split(m_gpuOrigPadRGB, rgbCh);
    for (auto& ch : rgbCh)
        cv::cuda::multiply(ch, invMask, ch);

    // ── 6. Write NCHW [R,G,B,mask] to input buffer  (D2D, zero-copy) ─────────
    float* inputPtr = static_cast<float*>(m_inputBuffer);

    for (int c = 0; c < 3; ++c) {
        cv::cuda::GpuMat inputPlane(EXPORT_H, EXPORT_W, CV_32FC1,
                                    inputPtr + c * planeSize);
        rgbCh[c].copyTo(inputPlane);
    }
    {
        cv::cuda::GpuMat maskPlane(EXPORT_H, EXPORT_W, CV_32FC1,
                                   inputPtr + 3 * planeSize);
        m_gpuMaskPad.copyTo(maskPlane);
    }
    // No dynamic setInputShape — inference shape is always EXPORT_H x EXPORT_W.
}

// ─────────────────────────────────────────────────────────────────────────────
//  postprocess  -  fully on GPU
// ─────────────────────────────────────────────────────────────────────────────
cv::Mat LaMa::postprocess()
{
    const int planeSize = EXPORT_H * EXPORT_W;
    float* outputPtr = static_cast<float*>(m_outputBuffer);

    // ── 1. Reference TRT output buffer as GpuMat views (no copy) ────────────
    std::vector<cv::cuda::GpuMat> outCh(3);
    for (int c = 0; c < 3; ++c)
        outCh[c] = cv::cuda::GpuMat(EXPORT_H, EXPORT_W, CV_32FC1,
                                     outputPtr + c * planeSize);

    cv::cuda::GpuMat gpuPredicted;
    cv::cuda::merge(outCh, gpuPredicted);   // 3-channel HWC GpuMat

    // ── 2. Clip sigmoid output to [0, 1] ─────────────────────────────────────
    //  Values can marginally exceed [0,1] due to FP32 rounding in TRT.
    cv::cuda::threshold(gpuPredicted, gpuPredicted, 1.0, 1.0, cv::THRESH_TRUNC);
    cv::cuda::threshold(gpuPredicted, gpuPredicted, 0.0, 0.0, cv::THRESH_TOZERO);

    // ── 3. Compositing:  result = mask * predicted + (1 - mask) * original ──
    //
    //  Process per channel: multiply 1-channel mask with each channel of the
    //  3-channel image individually, then accumulate.
    //  Matches original Python (default.py):
    //    batch['inpainted'] = mask * predicted_image + (1 - mask) * image
    cv::cuda::GpuMat invMask;
    cv::cuda::addWeighted(m_gpuMaskPad, -1.0, m_gpuMaskPad, 0.0, 1.0, invMask);

    std::vector<cv::cuda::GpuMat> predCh(3), origCh(3), blendCh(3);
    cv::cuda::split(gpuPredicted,    predCh);
    cv::cuda::split(m_gpuOrigPadRGB, origCh);

    for (int c = 0; c < 3; ++c) {
        cv::cuda::GpuMat t1, t2;
        cv::cuda::multiply(m_gpuMaskPad, predCh[c], t1);   // mask * pred
        cv::cuda::multiply(invMask,      origCh[c], t2);   // (1-mask) * orig
        cv::cuda::add(t1, t2, blendCh[c]);
    }

    cv::cuda::GpuMat blended;
    cv::cuda::merge(blendCh, blended);   // float32 3ch [0,1] RGB

    // ── 4. [0,1] -> [0,255],  convert to uint8 ───────────────────────────────
    cv::cuda::GpuMat resultU8;
    blended.convertTo(resultU8, CV_8UC3, 255.0);

    // ── 5. RGB -> BGR ─────────────────────────────────────────────────────────
    cv::cuda::GpuMat resultBGR;
    cv::cuda::cvtColor(resultU8, resultBGR, cv::COLOR_RGB2BGR);

    // ── 6. Download to CPU and resize back to original resolution ────────────
    cv::Mat result;
    resultBGR.download(result);
    if (result.rows == m_origH && result.cols == m_origW)
        return result;
    cv::Mat resized;
    cv::resize(result, resized, cv::Size(m_origW, m_origH), 0, 0, cv::INTER_LINEAR);
    return resized;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
inline void LaMa::checkCudaErrorCode(cudaError_t code)
{
    if (code != cudaSuccess)
        throw std::runtime_error(
            std::string("[LaMa] CUDA error: ") +
            cudaGetErrorName(code) + " - " + cudaGetErrorString(code)
        );
}
