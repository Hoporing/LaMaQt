#pragma once

#include <onnxruntime_cxx_api.h>
#include <cuda_runtime.h>

#include <string>
#include <vector>

// ============================================================================
//  OnnxEngine  -  ONNX Runtime inference engine with CUDA Execution Provider
//
//  TensorRT 10.x does not support the ONNX DFT operator used in LaMa's
//  18 FFC blocks.  This class replaces TensorRT with ONNX Runtime, which
//  natively supports DFT and runs on the CUDA EP for GPU acceleration.
//
//  Design:
//    - GPU I/O buffers are allocated via cudaMalloc and owned by this class.
//    - Callers write to getInputBuffer() / read from getOutputBuffer()
//      using cv::cuda::GpuMat views (device-to-device, zero host copy).
//    - The CUDA EP DLL (onnxruntime_providers_cuda.dll) is loaded dynamically
//      at runtime; no CUDA-compiled code is required in this translation unit.
//
//  Runtime DLLs required alongside the executable:
//    onnxruntime.dll
//    onnxruntime_providers_cuda.dll
//    onnxruntime_providers_shared.dll
// ============================================================================

class OnnxEngine {
public:
    struct Options {
        int deviceIndex = 0;
    };

    explicit OnnxEngine(const Options& options = {});
    ~OnnxEngine();

    // Load ONNX model and initialise the CUDA Execution Provider.
    // Returns true on success; prints error to stderr on failure.
    bool load(const std::string& onnxPath);

    // Run one synchronous inference pass.
    // The caller must have written input data to getInputBuffer() first.
    // Output is readable from getOutputBuffer() after this call returns.
    bool runInference();

    // Raw CUDA device-memory pointers (valid after load()).
    void* getInputBuffer()  const { return m_inputGpu; }
    void* getOutputBuffer() const { return m_outputGpu; }

private:
    Ort::Env m_env{ ORT_LOGGING_LEVEL_WARNING, "OnnxEngine" };
    std::unique_ptr<Ort::Session> m_session;

    void* m_inputGpu  = nullptr;
    void* m_outputGpu = nullptr;

    std::vector<int64_t> m_inputShape;
    std::vector<int64_t> m_outputShape;
    size_t m_inputElements  = 0;
    size_t m_outputElements = 0;

    std::string m_inputName;
    std::string m_outputName;

    Options m_options;
};
