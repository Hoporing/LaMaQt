#include "LaMaBridge.h"
#include "LaMaEngine.h"
#include "ResultImageProvider.h"

#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QImage>

LaMaBridge::LaMaBridge(QObject *parent)
    : QObject(parent)
    , m_engine(std::make_unique<LaMaEngine>())
{
}

LaMaBridge::~LaMaBridge() = default;

bool LaMaBridge::isReady() const
{
    return m_engine && m_engine->isReady();
}

void LaMaBridge::setModelPath(const QString &path)
{
    if (m_modelPath == path) return;
    m_modelPath = path;
    emit modelPathChanged();
}

void LaMaBridge::setResultProvider(ResultImageProvider *provider)
{
    m_resultProvider = provider;
}

// ── initialize ───────────────────────────────────────────────────────────────

void LaMaBridge::initialize(const QString &onnxPath)
{
    if (m_processing) return;

    setModelPath(onnxPath);
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
            emit errorOccurred("Failed to load model — check ONNX path.");
    });

    LaMaEngine *engine = m_engine.get();
    watcher->setFuture(QtConcurrent::run([engine, onnxPath]() -> bool {
        return engine->initialize(onnxPath);
    }));
}

// ── inpaint ──────────────────────────────────────────────────────────────────

void LaMaBridge::inpaint(const QString &imageBase64, const QString &maskBase64)
{
    if (!isReady() || m_processing) {
        if (!isReady())
            emit errorOccurred("Model not loaded — initialize first.");
        return;
    }

    // Decode images on the calling (UI) thread; decoding is fast
    QByteArray imgData  = QByteArray::fromBase64(imageBase64.toUtf8());
    QByteArray maskData = QByteArray::fromBase64(maskBase64.toUtf8());

    QImage sourceImg, maskImg;
    if (!sourceImg.loadFromData(imgData) || !maskImg.loadFromData(maskData)) {
        emit errorOccurred("Failed to decode image or mask data.");
        return;
    }

    m_processing = true;
    emit processingChanged();

    struct InpaintResult { QImage image; bool ok; };

    auto *watcher = new QFutureWatcher<InpaintResult>(this);

    connect(watcher, &QFutureWatcher<InpaintResult>::finished,
            this, [this, watcher]()
    {
        auto res = watcher->result();
        watcher->deleteLater();

        m_processing = false;
        emit processingChanged();

        if (!res.ok || res.image.isNull()) {
            emit errorOccurred("Inpainting failed — see console for details.");
            return;
        }

        if (m_resultProvider) {
            m_resultProvider->updateResult(res.image);
            emit inpaintCompleted(m_resultProvider->version());
        }
    });

    LaMaEngine *engine = m_engine.get();
    watcher->setFuture(QtConcurrent::run([engine, sourceImg, maskImg]() -> InpaintResult {
        QImage result = engine->inpaint(sourceImg, maskImg);
        return { result, !result.isNull() };
    }));
}
