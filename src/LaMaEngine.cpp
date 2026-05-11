#include "LaMaEngine.h"
#include "LaMa.h"   // from D:/Temporary/Class_ref (added to include path in CMakeLists)

#include <opencv2/opencv.hpp>
#include <QBuffer>

// ── Image conversion helpers ─────────────────────────────────────────────────

namespace {

// Convert QImage (any format) to cv::Mat BGR CV_8UC3
cv::Mat qImageToBGR(const QImage &img)
{
    QImage rgb = img.convertToFormat(QImage::Format_RGB888);
    // QImage::Format_RGB888 = R G B packed, 3 bytes per pixel
    cv::Mat rgbMat(rgb.height(), rgb.width(), CV_8UC3,
                   const_cast<uchar *>(rgb.bits()),
                   static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgrMat;
    cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
    return bgrMat.clone();  // clone: rgb is a local QImage whose bits() would dangle
}

// Convert cv::Mat BGR CV_8UC3 to QImage RGB888
QImage bgrToQImage(const cv::Mat &mat)
{
    cv::Mat rgbMat;
    cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
    return QImage(rgbMat.data,
                  rgbMat.cols, rgbMat.rows,
                  static_cast<qsizetype>(rgbMat.step[0]),
                  QImage::Format_RGB888).copy();
}

// Convert QImage mask (ARGB32 or grayscale) to cv::Mat Grayscale CV_8UC1.
// ARGB32: uses alpha channel — alpha > 0 = inpaint (white), else black.
// Grayscale: direct copy.
cv::Mat qImageToMask(const QImage &img)
{
    if (img.format() == QImage::Format_ARGB32 ||
        img.format() == QImage::Format_ARGB32_Premultiplied)
    {
        // Convert alpha channel to grayscale mask
        cv::Mat gray(img.height(), img.width(), CV_8UC1);
        for (int y = 0; y < img.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
            for (int x = 0; x < img.width(); ++x)
                gray.at<uchar>(y, x) = (qAlpha(line[x]) > 0) ? 255u : 0u;
        }
        return gray;
    }

    QImage g8 = img.convertToFormat(QImage::Format_Grayscale8);
    return cv::Mat(g8.height(), g8.width(), CV_8UC1,
                   const_cast<uchar *>(g8.bits()),
                   static_cast<size_t>(g8.bytesPerLine())).clone();
}

} // namespace

// ── LaMaEngine ───────────────────────────────────────────────────────────────

LaMaEngine::LaMaEngine() = default;

LaMaEngine::~LaMaEngine() = default;

bool LaMaEngine::initialize(const QString &onnxPath)
{
    m_ready = false;
    m_lama  = std::make_unique<LaMa>(onnxPath.toStdString());
    m_ready = m_lama->isReady();
    return m_ready;
}

QImage LaMaEngine::inpaint(const QImage &source, const QImage &mask)
{
    if (!m_ready || !m_lama) return {};

    cv::Mat bgrMat  = qImageToBGR(source);
    cv::Mat maskMat = qImageToMask(mask);

    cv::Mat result = m_lama->inpaint(bgrMat, maskMat);
    if (result.empty()) return {};

    return bgrToQImage(result);
}
