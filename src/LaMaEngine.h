#pragma once

#include <QImage>
#include <QString>
#include <memory>

// Forward declaration — actual type from D:/Temporary/Class_ref/LaMa.h
class LaMa;

// ============================================================================
//  LaMaEngine  -  Qt wrapper around the ONNX Runtime LaMa inpainting class
//
//  Handles:
//    - QImage <-> cv::Mat conversion (BGR/RGB/mask)
//    - LaMa lifecycle (initialize, inpaint)
//    - Provides a Qt-friendly interface for LaMaBridge
// ============================================================================
class LaMaEngine
{
public:
    LaMaEngine();
    ~LaMaEngine();

    // Load ONNX model.
    bool initialize(const QString &onnxPath);

    bool isReady() const { return m_ready; }

    // Run inpainting.
    //   source : RGB888 or ARGB32 source image (any resolution up to 2048x2048)
    //   mask   : grayscale or ARGB32 mask (white/alpha > 0 = region to inpaint)
    // Returns:
    //   Inpainted QImage (RGB888, same size as source), or null QImage on error.
    QImage inpaint(const QImage &source, const QImage &mask);

private:
    std::unique_ptr<LaMa> m_lama;
    bool                  m_ready = false;
};
