#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QPointF>
#include <QtQml/qqml.h>

#include <vector>

// ============================================================================
//  SegmentCanvas  -  Click-based segmentation canvas (QQuickPaintedItem)
//
//  Displays a source image letterboxed, accepts click prompts, and renders
//  the SAM-generated mask overlay and point markers.
//
//  Interaction:
//    Left-click  → positive prompt (foreground, green dot)
//    Right-click → negative prompt (background, red dot)
//
//  Signals:
//    pointAdded(x, y, isPositive) — image-space pixel coords
//      → connect to SAMBridge.addPoint(x, y, isPositive) in QML
//
//  Mask display:
//    setMaskOverlay(base64) — Grayscale8 PNG → rendered as blue overlay
//    Pass "" or call clear() to remove the overlay.
// ============================================================================

class SegmentCanvas : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal maskOpacity READ maskOpacity WRITE setMaskOpacity NOTIFY maskOpacityChanged)
    Q_PROPERTY(bool  hasImage    READ hasImage    NOTIFY hasImageChanged)
    Q_PROPERTY(bool  hasMask     READ hasMask     NOTIFY hasMaskChanged)
    Q_PROPERTY(bool  boxMode     READ boxMode     WRITE setBoxMode     NOTIFY boxModeChanged)

public:
    explicit SegmentCanvas(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    qreal maskOpacity() const { return m_maskOpacity; }
    bool  hasImage()    const { return !m_baseImage.isNull(); }
    bool  hasMask()     const { return !m_maskOverlay.isNull(); }
    bool  boxMode()     const { return m_boxMode; }

    void setMaskOpacity(qreal opacity);
    void setBoxMode(bool mode);

    // ── QML invokable ────────────────────────────────────────────────────────

    // Load source image from base64 PNG; resets all points and mask overlay.
    Q_INVOKABLE void setBaseImage(const QString &base64);

    // Update the mask overlay from a Grayscale8 PNG base64.
    // Pass empty string to clear the overlay.
    Q_INVOKABLE void setMaskOverlay(const QString &maskBase64);

    // Remove the last point marker (visual only — SAMBridge manages actual points).
    Q_INVOKABLE void undoLastPoint();

    // Clear all point markers and mask overlay (visual reset).
    Q_INVOKABLE void clear();

    // Clear the box overlay (visual reset).
    Q_INVOKABLE void clearBox();

    // Returns the source image as base64 PNG (for LaMaBridge.inpaint).
    Q_INVOKABLE QString getImageBase64() const;

signals:
    void maskOpacityChanged();
    void hasImageChanged();
    void hasMaskChanged();
    void boxModeChanged();

    // Emitted on click with image-space pixel coordinates.
    void pointAdded(qreal x, qreal y, bool isPositive);

    // Emitted when a drag-box is confirmed (image-space pixel coordinates).
    void boxSet(qreal x1, qreal y1, qreal x2, qreal y2);

protected:
    void mousePressEvent(QMouseEvent   *event) override;
    void mouseMoveEvent(QMouseEvent    *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent    *event) override;
    void hoverLeaveEvent(QHoverEvent   *event) override;

private:
    QRectF  imageRect()                         const;
    QPointF toImageCoords(const QPointF &itemPos) const;
    bool    isInsideImage(const QPointF &itemPos) const;

    QImage  m_baseImage;
    QImage  m_maskOverlay;   // ARGB32, blue tint over mask region

    struct DisplayPoint { QPointF imageCoords; bool isPositive; };
    std::vector<DisplayPoint> m_displayPoints;

    // Box prompt state
    bool    m_boxMode        = false;
    bool    m_isDragging     = false;
    QPointF m_dragStartItem;
    QPointF m_dragCurrItem;
    struct DisplayBox { QPointF imgP1, imgP2; bool valid = false; };
    DisplayBox m_displayBox;

    qreal   m_maskOpacity = 0.50;
    bool    m_showCursor  = false;
    QPointF m_cursorPos;
};
