#include "MaskPainter.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QBuffer>
#include <QLineF>
#include <cmath>

MaskPainter::MaskPainter(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);

    // Ensure mouse events propagate correctly inside Qt Quick
    setFlag(QQuickItem::ItemAcceptsInputMethod, false);
}

// ── Letterbox geometry ────────────────────────────────────────────────────────

QRectF MaskPainter::imageRect() const
{
    if (m_baseImage.isNull()) return boundingRect();

    const double iw = m_baseImage.width();
    const double ih = m_baseImage.height();
    const double bw = width();
    const double bh = height();

    const double scale = qMin(bw / iw, bh / ih);
    const double dw    = iw * scale;
    const double dh    = ih * scale;

    return QRectF((bw - dw) / 2.0, (bh - dh) / 2.0, dw, dh);
}

QPointF MaskPainter::toImageCoords(const QPointF &itemPos) const
{
    if (m_baseImage.isNull()) return {};
    QRectF r = imageRect();
    if (r.width() <= 0.0 || r.height() <= 0.0) return {};

    const double x = (itemPos.x() - r.x()) / r.width()  * m_baseImage.width();
    const double y = (itemPos.y() - r.y()) / r.height() * m_baseImage.height();
    return QPointF(x, y);
}

// ── Drawing ───────────────────────────────────────────────────────────────────

void MaskPainter::drawBrushAt(const QPointF &itemPos)
{
    if (m_overlay.isNull()) return;

    // Convert brush size from screen pixels to image pixels
    QRectF r = imageRect();
    double scale = (r.width() > 0.0) ? r.width() / m_baseImage.width() : 1.0;
    double radius = (m_brushSize / 2.0) / scale;  // image-space radius

    QPainter p(&m_overlay);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_eraseMode) {
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.setBrush(Qt::transparent);
        p.setPen(Qt::NoPen);
    } else {
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.setBrush(QColor(255, 70, 70, 220));  // semi-transparent red
        p.setPen(Qt::NoPen);
    }

    QPointF imgPos = toImageCoords(itemPos);

    // Interpolate from last position to current for smooth stroke
    if (m_drawing && !m_lastPaintPos.isNull()) {
        QPointF lastImg = toImageCoords(m_lastPaintPos);
        QLineF  stroke(lastImg, imgPos);
        double  len  = stroke.length();
        double  step = qMax(1.0, radius * 0.5);  // step = half radius for dense fill

        for (double t = 0.0; t <= len + step * 0.5; t += step) {
            double  ratio = (len > 0.0) ? qMin(t / len, 1.0) : 0.0;
            QPointF pt    = lastImg + (imgPos - lastImg) * ratio;
            p.drawEllipse(pt, radius, radius);
        }
    } else {
        p.drawEllipse(imgPos, radius, radius);
    }

    if (!m_eraseMode && !m_hasContent) {
        m_hasContent = true;
        emit hasContentChanged();
    }

    update();
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void MaskPainter::mousePressEvent(QMouseEvent *event)
{
    if (m_baseImage.isNull() || event->button() != Qt::LeftButton) return;

    m_drawing      = true;
    m_lastPaintPos = QPointF();
    drawBrushAt(event->position());
    m_lastPaintPos = event->position();
    event->accept();
}

void MaskPainter::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing || m_baseImage.isNull()) return;

    drawBrushAt(event->position());
    m_lastPaintPos = event->position();
    event->accept();
}

void MaskPainter::mouseReleaseEvent(QMouseEvent *event)
{
    m_drawing      = false;
    m_lastPaintPos = QPointF();
    event->accept();
}

// ── Hover (brush cursor) ──────────────────────────────────────────────────────

void MaskPainter::hoverMoveEvent(QHoverEvent *event)
{
    m_cursorPos   = event->position();
    m_showCursor  = true;
    update();
    QQuickPaintedItem::hoverMoveEvent(event);
}

void MaskPainter::hoverLeaveEvent(QHoverEvent *event)
{
    m_showCursor = false;
    update();
    QQuickPaintedItem::hoverLeaveEvent(event);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void MaskPainter::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), QColor(30, 30, 30));

    if (m_baseImage.isNull()) {
        painter->setPen(QColor(90, 90, 90));
        painter->setFont(QFont("Segoe UI", 11));
        painter->drawText(boundingRect(), Qt::AlignCenter,
                          "Capture a video frame\nor open an image file");
        return;
    }

    QRectF r = imageRect();

    // 1. Draw source image
    painter->drawImage(r, m_baseImage);

    // 2. Draw mask overlay
    if (!m_overlay.isNull() && m_hasContent) {
        painter->setOpacity(m_maskOpacity);
        painter->drawImage(r, m_overlay);
        painter->setOpacity(1.0);
    }

    // 3. Draw brush cursor (white circle outline)
    if (m_showCursor && !m_baseImage.isNull()) {
        QPen cursorPen(m_eraseMode ? QColor(100, 200, 255, 200)
                                   : QColor(255, 255, 255, 200), 1.5);
        painter->setPen(cursorPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(m_cursorPos,
                             m_brushSize / 2.0,
                             m_brushSize / 2.0);
    }
}

// ── QML invokable ─────────────────────────────────────────────────────────────

void MaskPainter::setBaseImage(const QString &base64)
{
    QByteArray ba = QByteArray::fromBase64(base64.toUtf8());
    QImage img;
    if (!img.loadFromData(ba)) return;

    m_baseImage  = img.convertToFormat(QImage::Format_RGB888);
    m_overlay    = QImage(m_baseImage.size(), QImage::Format_ARGB32);
    m_overlay.fill(Qt::transparent);
    m_hasContent = false;

    emit hasContentChanged();
    emit hasImageChanged();
    update();
}

void MaskPainter::clear()
{
    if (m_overlay.isNull()) return;
    m_overlay.fill(Qt::transparent);
    m_hasContent = false;
    emit hasContentChanged();
    update();
}

QString MaskPainter::getMaskBase64() const
{
    if (m_overlay.isNull()) return {};

    // Convert ARGB32 overlay → Grayscale8 B&W mask
    // Rule: alpha > 0 (any red painted pixel) → white (inpaint)
    //       alpha == 0 (transparent / erased)  → black (keep)
    QImage mask(m_overlay.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < m_overlay.height(); ++y) {
        const QRgb *src = reinterpret_cast<const QRgb *>(m_overlay.constScanLine(y));
        uchar       *dst = mask.scanLine(y);
        for (int x = 0; x < m_overlay.width(); ++x)
            dst[x] = (qAlpha(src[x]) > 0) ? 255u : 0u;
    }

    QByteArray ba;
    QBuffer    buf(&ba);
    buf.open(QIODevice::WriteOnly);
    mask.save(&buf, "PNG");
    return ba.toBase64();
}

QString MaskPainter::getImageBase64() const
{
    if (m_baseImage.isNull()) return {};

    QByteArray ba;
    QBuffer    buf(&ba);
    buf.open(QIODevice::WriteOnly);
    m_baseImage.save(&buf, "PNG");
    return ba.toBase64();
}

// ── Property setters ──────────────────────────────────────────────────────────

void MaskPainter::setBrushSize(int size)
{
    size = qBound(1, size, 200);
    if (m_brushSize == size) return;
    m_brushSize = size;
    emit brushSizeChanged();
    update();
}

void MaskPainter::setEraseMode(bool erase)
{
    if (m_eraseMode == erase) return;
    m_eraseMode = erase;
    emit eraseModeChanged();
    update();
}

void MaskPainter::setMaskOpacity(qreal opacity)
{
    opacity = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_maskOpacity, opacity)) return;
    m_maskOpacity = opacity;
    emit maskOpacityChanged();
    update();
}
