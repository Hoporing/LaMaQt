#pragma once

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QtQml/qqml.h>

#include <memory>
#include <vector>

class ISAMEngine;

// ============================================================================
//  SAMBridge  -  QML bridge for SAM 2 segmentation
//
//  Typical QML flow:
//    samBridge.initialize(encoderPath, decoderPath)  // once at startup
//    samBridge.setImage(imageBase64)                 // on image load (async encode)
//    samBridge.addPoint(x, y, true)                  // on click (async decode)
//    onMaskReady(base64) -> segmentCanvas.setMaskOverlay(base64)
//    samBridge.getMaskBase64()                        // when sending to LaMa
//    samBridge.clearPoints()                          // reset
//
//  Threading:
//    initialize() and setImage() are slow (model load / encoder) → async.
//    decode() is fast (~100ms GPU) but also async to keep UI responsive.
//    Only one heavy operation runs at a time (m_processing flag).
//    If addPoint() arrives while decoding, m_decodePending queues a re-run.
// ============================================================================

class SAMBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool ready        READ isReady        NOTIFY readyChanged)
    Q_PROPERTY(bool processing   READ isProcessing   NOTIFY processingChanged)
    Q_PROPERTY(bool hasMask      READ hasMask        NOTIFY hasMaskChanged)
    Q_PROPERTY(bool hasImage     READ hasImage       NOTIFY hasImageChanged)
    Q_PROPERTY(bool hasBox       READ hasBox         NOTIFY hasBoxChanged)
    Q_PROPERTY(int  dilateRadius READ dilateRadius   WRITE setDilateRadius NOTIFY dilateRadiusChanged)

public:
    explicit SAMBridge(QObject *parent = nullptr);
    ~SAMBridge() override;

    bool isReady()      const;
    bool isProcessing() const { return m_processing; }
    bool hasMask()      const { return !m_currentMask.isNull(); }
    bool hasImage()     const { return m_imageEncoded; }
    bool hasBox()       const { return m_box.valid; }
    int  dilateRadius() const { return m_dilateRadius; }

    void setDilateRadius(int r);

    // ── QML invokable ────────────────────────────────────────────────────────

    // Load encoder + decoder ONNX models (async).
    Q_INVOKABLE void initialize(const QString &encoderPath,
                                const QString &decoderPath);

    // Encode a new source image (async — runs SAM encoder, ~1-2s on GPU).
    // Clears all existing points and the current mask.
    Q_INVOKABLE void setImage(const QString &imageBase64);

    // Add a prompt point and trigger decode (async).
    // x, y: image-space pixel coordinates (from SegmentCanvas.pointAdded).
    // isPositive: true = foreground, false = background.
    Q_INVOKABLE void addPoint(qreal x, qreal y, bool isPositive);

    // Remove the last added point and re-decode.
    Q_INVOKABLE void undoPoint();

    // Set a box prompt and trigger decode (async).
    // x1,y1 = top-left;  x2,y2 = bottom-right, image-space pixel coords.
    Q_INVOKABLE void setBox(qreal x1, qreal y1, qreal x2, qreal y2);

    // Remove the box prompt and re-decode (or clear mask if no points either).
    Q_INVOKABLE void clearBox();

    // Clear all points and the current mask.
    Q_INVOKABLE void clearPoints();

    // Returns the current mask as base64 PNG (Grayscale8, white=inpaint).
    // Empty string if no mask is available.
    Q_INVOKABLE QString getMaskBase64() const;

signals:
    void readyChanged();
    void processingChanged();
    void hasMaskChanged();
    void hasImageChanged();

    // Emitted after initialize() completes.
    void initializeDone(bool success);

    // Emitted after setImage() encoding completes.
    void encodingDone(bool success);

    // Emitted after each decode with the mask as base64 PNG (Grayscale8).
    void maskReady(const QString &maskBase64);

    void errorOccurred(const QString &message);
    void dilateRadiusChanged();
    void hasBoxChanged();

private:
    void runDecode();

    std::unique_ptr<ISAMEngine> m_engine;

    bool m_processing    = false;
    bool m_decodePending = false;  // re-run decode after current finishes
    bool m_imageEncoded  = false;
    int  m_dilateRadius  = 20;     // mask expansion before passing to LaMa

    struct PromptPoint { float x, y; int label; };  // label: 1=pos, 0=neg
    std::vector<PromptPoint> m_points;

    struct BoxRegion { float x1, y1, x2, y2; bool valid = false; };
    BoxRegion m_box;

    mutable QMutex m_maskMutex;
    QImage         m_currentMask;   // Grayscale8, kept for getMaskBase64()
};
