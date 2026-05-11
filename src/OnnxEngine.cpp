#include "OnnxEngine.h"

#include <iostream>
#include <stdexcept>

OnnxEngine::OnnxEngine(const Options& options) : m_options(options) {}

OnnxEngine::~OnnxEngine() {
    if (m_inputGpu)  { cudaFree(m_inputGpu);  m_inputGpu  = nullptr; }
    if (m_outputGpu) { cudaFree(m_outputGpu); m_outputGpu = nullptr; }
}

bool OnnxEngine::load(const std::string& onnxPath) {
    try {
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetGraphOptimizationLevel(ORT_ENABLE_ALL);

        // Attach CUDA Execution Provider
        OrtCUDAProviderOptions cudaOptions{};
        cudaOptions.device_id = m_options.deviceIndex;
        sessionOptions.AppendExecutionProvider_CUDA(cudaOptions);

        // Load model (Windows requires wchar_t path)
        std::wstring wPath(onnxPath.begin(), onnxPath.end());
        m_session = std::make_unique<Ort::Session>(m_env, wPath.c_str(), sessionOptions);

        // Query tensor names
        Ort::AllocatorWithDefaultOptions allocator;
        auto inputNamePtr  = m_session->GetInputNameAllocated(0, allocator);
        auto outputNamePtr = m_session->GetOutputNameAllocated(0, allocator);
        m_inputName  = inputNamePtr.get();
        m_outputName = outputNamePtr.get();

        // Query shapes
        m_inputShape  = m_session->GetInputTypeInfo(0)
                            .GetTensorTypeAndShapeInfo().GetShape();
        m_outputShape = m_session->GetOutputTypeInfo(0)
                            .GetTensorTypeAndShapeInfo().GetShape();

        m_inputElements = 1;
        for (auto d : m_inputShape)  m_inputElements  *= static_cast<size_t>(d);
        m_outputElements = 1;
        for (auto d : m_outputShape) m_outputElements *= static_cast<size_t>(d);

        // Allocate GPU I/O buffers (owned by this engine)
        if (cudaMalloc(&m_inputGpu,  m_inputElements  * sizeof(float)) != cudaSuccess ||
            cudaMalloc(&m_outputGpu, m_outputElements * sizeof(float)) != cudaSuccess)
            throw std::runtime_error("cudaMalloc failed for I/O buffers.");

        std::cout << "[OnnxEngine] Loaded: " << onnxPath << "\n"
                  << "  Input  \"" << m_inputName  << "\" ["
                  << m_inputShape[0]  << "," << m_inputShape[1]  << ","
                  << m_inputShape[2]  << "," << m_inputShape[3]  << "]\n"
                  << "  Output \"" << m_outputName << "\" ["
                  << m_outputShape[0] << "," << m_outputShape[1] << ","
                  << m_outputShape[2] << "," << m_outputShape[3] << "]\n";
        return true;
    }
    catch (const Ort::Exception& e) {
        std::cerr << "[OnnxEngine] Load failed: " << e.what() << "\n";
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "[OnnxEngine] Load failed: " << e.what() << "\n";
        return false;
    }
}

bool OnnxEngine::runInference() {
    try {
        // Wrap the pre-allocated CUDA buffers as ORT tensors (zero-copy view).
        // Ort::Value::CreateTensor does NOT copy data — it creates a view over
        // the existing device memory.
        auto cudaMemInfo = Ort::MemoryInfo(
            "Cuda", OrtDeviceAllocator, m_options.deviceIndex, OrtMemTypeDefault);

        auto inputTensor = Ort::Value::CreateTensor(
            cudaMemInfo,
            static_cast<float*>(m_inputGpu), m_inputElements,
            m_inputShape.data(), m_inputShape.size());

        auto outputTensor = Ort::Value::CreateTensor(
            cudaMemInfo,
            static_cast<float*>(m_outputGpu), m_outputElements,
            m_outputShape.data(), m_outputShape.size());

        // IoBinding keeps the entire pipeline on GPU (no host round-trips).
        Ort::IoBinding binding(*m_session);
        binding.BindInput (m_inputName.c_str(),  inputTensor);
        binding.BindOutput(m_outputName.c_str(), outputTensor);

        m_session->Run(Ort::RunOptions{nullptr}, binding);
        binding.SynchronizeOutputs();   // block until GPU kernels complete
        return true;
    }
    catch (const Ort::Exception& e) {
        std::cerr << "[OnnxEngine] Inference failed: " << e.what() << "\n";
        return false;
    }
}
