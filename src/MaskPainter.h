#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QPointF>
#include <QtQml/qqml.h>

// ============================================================================
//  MaskPainter  -  Interactive mask drawing canvas (QQuickPaintedItem)
//
//  Displays a source image as background and lets the user paint a mask on top.
//  The mask is stored in image-space coordinates (same resolution as source image),
//  so result quality is independent of window size.
//
//  Rendering:
//    - Source image: displayed letterboxed (aspect-ratio preserved)
//    - Mask overlay: semi-transparent red (draw) / transparent (erased)
//    - Brush cursor: white circle outline showing brush size
//
//  Export:
//    getMaskBase64()  -> B&W PNG (white = inpaint, black = keep)
//    getImageBase64() -> source image PNG for LaMaBridge.inpaint()
// ============================================================================
class MaskPainter : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int   brushSize   READ brushSize   WRITE setBrushSize   NOTIFY brushSizeChanged)
    Q_PROPERTY(bool  eraseMode   READ eraseMode   WRITE setEraseMode   NOTIFY eraseModeChanged)
    Q_PROPERTY(qreal maskOpacity READ maskOpacity WRITE setMaskOpacity NOTIFY maskOpacityChanged)
    Q_PROPERTY(bool  hasContent  READ hasContent  NOTIFY hasContentChanged)
    Q_PROPERTY(bool  hasImage    READ hasImage    NOTIFY hasImageChanged)

public:
    explicit MaskPainter(QQuickItem *parent = nullptr);

    // ── QQuickPaintedItem override ──────────────────────────────
    void paint(QPainter *painter) override;

    // ── Properties ──────────────────────────────────────────────
    int   brushSize()   const { return m_brushSize; }
    bool  eraseMode()   const { return m_eraseMode; }
    qreal maskOpacity() const { return m_maskOpacity; }
    bool  hasContent()  const { return m_hasContent; }
    bool  hasImage()    const { return !m_baseImage.isNull(); }

    void setBrushSize(int size);
    void setEraseMode(bool erase);
    void setMaskOpacity(qreal opacity);

    // ── QML invokable ────────────────────────────────────────────
    // Load source image from base64 PNG and reset the mask.
    Q_INVOKABLE void setBaseImage(const QString &base64);

    // Clear the mask (keep source image).
    Q_INVOKABLE void clear();

    // Returns B&W PNG base64 mask (white = inpaint, black = keep).
    Q_INVOKABLE QString getMaskBase64() const;

    // Returns source image PNG base64 for inpainting.
    Q_INVOKABLE QString getImageBase64() const;

signals:
    void brushSizeChanged();
    void eraseModeChanged();
    void maskOpacityChanged();
    void hasContentChanged();
    void hasImageChanged();

protected:
    void mousePressEvent(QMouseEvent  *event) override;
    void mouseMoveEvent(QMouseEvent   *event) override;
    void mouseReleaseEvent(QMouseEvent*event) override;
    void hoverMoveEvent(QHoverEvent   *event) override;
    void hoverLeaveEvent(QHoverEvent  *event) override;

private:
    // Returns the letterbox rect of m_baseImage within the item bounds.
    QRectF  imageRect() const;

    // Convert item-space position to image-space position.
    QPointF toImageCoords(const QPointF &itemPos) const;

    // Draw (or erase) a brush stroke at itemPos, interpolating from m_lastPaintPos.
    void drawBrushAt(const QPointF &itemPos);

    // ── Data ────────────────────────────────────────────────────
    QImage  m_baseImage;  // Source image (RGB888) — displayed as background
    QImage  m_overlay;    // ARGB32 overlay: transparent = keep, red = inpaint

    int     m_brushSize   = 25;
    bool    m_eraseMode   = false;
    qreal   m_maskOpacity = 0.65;
    bool    m_hasContent  = false;

    bool    m_drawing     = false;
    QPointF m_lastPaintPos;

    // Hover cursor
    bool    m_showCursor  = false;
    QPointF m_cursorPos;
};
