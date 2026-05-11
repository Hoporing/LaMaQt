#include "SAMBridge.h"
#include "SAM2Engine.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QBuffer>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

SAMBridge::SAMBridge(QObject *parent)
    : QObject(parent)
    , m_engine(std::make_unique<SAM2Engine>())
{
}

SAMBridge::~SAMBridge() = default;

bool SAMBridge::isReady() const
{
    return m_engine && m_engine->isReady();
}

// ─────────────────────────────────────────────────────────────────────────────
//  initialize
// ─────────────────────────────────────────────────────────────────────────────
void SAMBridge::initialize(const QString &encoderPath, const QString &decoderPath)
{
    if (m_processing) return;

    m_processing = true;
    emit processingChanged();

    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        bool ok = watcher->result();
        watcher->deleteLater();

        m_processing = false;
        emit processingChanged();
        emit readyChanged();
        emit initializeDone(ok);

        if (!ok)
            emit errorOccurred("SAM model load failed — check ONNX paths.");
    });

    ISAMEngine *engine = m_engine.get();
    watcher->setFuture(QtConcurrent::run([engine, encoderPath, decoderPath]() -> bool {
        return engine->loadModels(encoderPath.toStdString(), decoderPath.toStdString());
    }));
}

// ─────────────────────────────────────────────────────────────────────────────
//  setImage
// ─────────────────────────────────────────────────────────────────────────────
void SAMBridge::setImage(const QString &imageBase64)
{
    if (!isReady()) {
        emit errorOccurred("SAM not initialized — call initialize() first.");
        return;
    }
    if (m_processing) return;

    // Reset state
    m_points.clear();
    m_box            = {};
    m_decodePending  = false;
    m_imageEncoded   = false;
    {
        QMutexLocker lk(&m_maskMutex);
        m_currentMask = QImage{};
    }
    emit hasBoxChanged();
    emit hasMaskChanged();
    emit hasImageChanged();

    // Decode base64 → cv::Mat (on UI thread, fast)
    QByteArray ba  = QByteArray::fromBase64(imageBase64.toUtf8());
    QImage     img;
    if (!img.loadFromData(ba)) {
        emit errorOccurred("SAMBridge: failed to decode image data.");
        return;
    }
    // Convert QImage → BGR cv::Mat
    QImage rgb = img.convertToFormat(QImage::Format_RGB888);
    cv::Mat bgrMat;
    {
        cv::Mat rgbMat(rgb.height(), rgb.width(), CV_8UC3,
                       const_cast<uchar *>(rgb.bits()),
                       static_cast<size_t>(rgb.bytesPerLine()));
        cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
    }

    m_processing = true;
    emit processingChanged();

    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        bool ok = watcher->result();
        watcher->deleteLater();

        m_processing   = false;
        m_imageEncoded = ok;
        emit processingChanged();
        emit hasImageChanged();
        emit encodingDone(ok);

        if (!ok)
            emit errorOccurred("SAM image encoding failed.");
    });

    ISAMEngine *engine = m_engine.get();
    watcher->setFuture(QtConcurrent::run([engine, bgrMat]() -> bool {
        return engine->encodeImage(bgrMat);
    }));
}

// ─────────────────────────────────────────────────────────────────────────────
//  addPoint
// ─────────────────────────────────────────────────────────────────────────────
void SAMBridge::addPoint(qreal x, qreal y, bool isPositive)
{
    if (!m_imageEncoded) return;

    m_points.push_back({ static_cast<float>(x),
                         static_cast<float>(y),
                         isPositive ? 1 : 0 });

    if (m_processing) {
        m_decodePending = true;
        return;
    }
    runDecode();
}

// ─────────────────────────────────────────────────────────────────────────────
//  undoPoint
// ─────────────────────────────────────────────────────────────────────────────
void SAMBridge::undoPoint()
{
    if (m_points.empty()) return;
    m_points.pop_back();

    if (m_points.empty()) {
        clearPoints();
        return;
    }
    if (m_processing) {
        m_decodePending = true;
        return;
    }
    runDecode();
}

// ─────────────────────────────────────────────────────────────────────────────
//  setDilateRadius
// ─────────────────────────────────────────────────────────────────────────────
void SAMBridge::setDilateRadius(int r)
{
    r = qBound(0, r, 200);
    if (m_dilateRadius == r) return;
    m_dilateRadius = r;
    emit dilateRadiusChanged();

    // Re-decode with new dilation if prompts exist
    if (!m_points.empty() || m_box.valid) {
        if (m_processing)
            m_decodePending = true;
        else
            runDecode();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  clearPoints
// ─────────────────────────────────────────────────────────────────────────────
void SAMBridge::clearPoints()
{
    m_points.clear();
    m_box           = {};
    m_decodePending = false;
    {
        QMutexLocker lk(&m_maskMutex);
        m_currentMask = QImage{};
    }
    emit hasBoxChanged();
    emit hasMaskChanged();
    emit maskReady(QString{});  // signal empty mask to clear canvas overlay
}

// ─────────────────────────────────────────────────────────────────────────────
//  setBox / clearBox
// ─────────────────────────────────────────────────────────────────────────────
void SAMBridge::setBox(qreal x1, qreal y1, qreal x2, qreal y2)
{
    if (!m_imageEncoded) return;

    m_box = { static_cast<float>(x1), static_cast<float>(y1),
              static_cast<float>(x2), static_cast<float>(y2), true };
    emit hasBoxChanged();

    if (m_processing)
        m_decodePending = true;
    else
        runDecode();
}

void SAMBridge::clearBox()
{
    if (!m_box.valid) return;
    m_box.valid = false;
    emit hasBoxChanged();

    if (m_points.empty()) {
        // Nothing left to decode — clear mask
        m_decodePending = false;
        {
            QMutexLocker lk(&m_maskMutex);
            m_currentMask = QImage{};
        }
        emit hasMaskChanged();
        emit maskReady(QString{});
    } else {
        // Re-decode with remaining points
        if (m_processing)
            m_decodePending = true;
        else
            runDecode();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  getMaskBase64
// ─────────────────────────────────────────────────────────────────────────────
QString SAMBridge::getMaskBase64() const
{
    QMutexLocker lk(&m_maskMutex);
    if (m_currentMask.isNull()) return {};

    QByteArray ba;
    QBuffer    buf(&ba);
    buf.open(QIODevice::WriteOnly);
    m_currentMask.save(&buf, "PNG");
    return QString::fromLatin1(ba.toBase64());
}

// ─────────────────────────────────────────────────────────────────────────────
//  runDecode  (internal)
// ─────────────────────────────────────────────────────────────────────────────
void SAMBridge::runDecode()
{
    if (m_points.empty() && !m_box.valid) return;

    // Snapshot current prompts and dilate radius for the worker thread
    const std::vector<PromptPoint> pts    = m_points;
    const BoxRegion                box    = m_box;
    const int                      dilate = m_dilateRadius;

    m_processing = true;
    emit processingChanged();

    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher]() {
        QImage mask = watcher->result();
        watcher->deleteLater();

        m_processing = false;
        emit processingChanged();

        if (!mask.isNull()) {
            {
                QMutexLocker lk(&m_maskMutex);
                m_currentMask = mask;
            }
            emit hasMaskChanged();

            // Encode mask to base64 PNG and notify QML
            QByteArray ba;
            QBuffer    buf(&ba);
            buf.open(QIODevice::WriteOnly);
            mask.save(&buf, "PNG");
            emit maskReady(QString::fromLatin1(ba.toBase64()));
        }

        // Re-run if a new point arrived while we were decoding
        if (m_decodePending) {
            m_decodePending = false;
            runDecode();
        }
    });

    ISAMEngine *engine = m_engine.get();
    watcher->setFuture(QtConcurrent::run([engine, pts, box, dilate]() -> QImage {
        std::vector<cv::Point2f> points;
        std::vector<int>         labels;
        points.reserve(pts.size() + (box.valid ? 2 : 0));
        labels.reserve(pts.size() + (box.valid ? 2 : 0));

        // Box corners go first: label 2 = top-left, label 3 = bottom-right
        if (box.valid) {
            points.push_back({ box.x1, box.y1 });  labels.push_back(2);
            points.push_back({ box.x2, box.y2 });  labels.push_back(3);
        }

        for (const auto &p : pts) {
            points.push_back({ p.x, p.y });
            labels.push_back(p.label);
        }

        cv::Mat maskMat;
        if (!engine->decode(points, labels, maskMat) || maskMat.empty())
            return {};

        // Expand mask boundary so LaMa has enough context around the object
        if (dilate > 0) {
            const int sz = dilate * 2 + 1;
            cv::Mat kernel = cv::getStructuringElement(
                cv::MORPH_ELLIPSE, cv::Size(sz, sz));
            cv::dilate(maskMat, maskMat, kernel);
        }

        // cv::Mat CV_8UC1 → QImage Grayscale8 (deep copy: Mat data freed after return)
        QImage img(maskMat.data, maskMat.cols, maskMat.rows,
                   static_cast<qsizetype>(maskMat.step[0]),
                   QImage::Format_Grayscale8);
        return img.copy();
    }));
}
