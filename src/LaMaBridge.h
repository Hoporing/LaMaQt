#pragma once

#include <QObject>
#include <QtQml/qqml.h>
#include <memory>

class LaMaEngine;
class ResultImageProvider;

// ============================================================================
//  LaMaBridge  -  QML element exposing LaMa inpainting to the UI
//
//  Usage in QML:
//    lamaBridge.initialize("C:/models/lama.onnx")
//    lamaBridge.inpaint(imageBase64, maskBase64)
//    onInpaintCompleted(version) -> update image://result/frame?v=version
// ============================================================================
class LaMaBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool    ready      READ isReady      NOTIFY readyChanged)
    Q_PROPERTY(bool    processing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(QString modelPath  READ modelPath    WRITE  setModelPath NOTIFY modelPathChanged)

public:
    explicit LaMaBridge(QObject *parent = nullptr);
    ~LaMaBridge() override;

    bool    isReady()      const;
    bool    isProcessing() const { return m_processing; }
    QString modelPath()    const { return m_modelPath; }

    void setModelPath(const QString &path);

    // Called from main.cpp after engine.addImageProvider("result", ...)
    void setResultProvider(ResultImageProvider *provider);

    // ── QML invokable ───────────────────────────────────────────────
    // Load ONNX model.
    Q_INVOKABLE void initialize(const QString &onnxPath);

    // Run async inpainting on thread pool.
    //   imageBase64 : PNG base64 of source image
    //   maskBase64  : PNG base64 of B&W mask (white = inpaint)
    Q_INVOKABLE void inpaint(const QString &imageBase64, const QString &maskBase64);

signals:
    void readyChanged();
    void processingChanged();
    void modelPathChanged();

    // Emitted when inpainting is done — version is the new ResultImageProvider version.
    // QML should update image source to "image://result/frame?v=" + version
    void inpaintCompleted(int version);

    // Emitted on initialization success/failure
    void initializeDone(bool success);

    void errorOccurred(const QString &message);

private:
    std::unique_ptr<LaMaEngine> m_engine;
    ResultImageProvider        *m_resultProvider = nullptr;
    bool                        m_processing      = false;
    QString                     m_modelPath;
};
