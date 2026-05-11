#include "SegmentCanvas.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QBuffer>

SegmentCanvas::SegmentCanvas(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setAcceptHoverEvents(true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Geometry helpers
// ─────────────────────────────────────────────────────────────────────────────

QRectF SegmentCanvas::imageRect() const
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

QPointF SegmentCanvas::toImageCoords(const QPointF &itemPos) const
{
    if (m_baseImage.isNull()) return {};
    const QRectF r = imageRect();
    if (r.width() <= 0.0 || r.height() <= 0.0) return {};

    const double x = (itemPos.x() - r.x()) / r.width()  * m_baseImage.width();
    const double y = (itemPos.y() - r.y()) / r.height() * m_baseImage.height();
    return QPointF(x, y);
}

bool SegmentCanvas::isInsideImage(const QPointF &itemPos) const
{
    return imageRect().contains(itemPos);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mouse events
// ─────────────────────────────────────────────────────────────────────────────

void SegmentCanvas::mousePressEvent(QMouseEvent *event)
{
    if (m_baseImage.isNull()) { event->ignore(); return; }
    if (!isInsideImage(event->position())) { event->ignore(); return; }

    if (m_boxMode && event->button() == Qt::LeftButton) {
        // Start drag — clear previous box
        m_isDragging       = true;
        m_dragStartItem    = event->position();
        m_dragCurrItem     = event->position();
        m_displayBox.valid = false;
        update();
        event->accept();
        return;
    }

    const bool isPositive = (event->button() == Qt::LeftButton);
    const QPointF imgCoords = toImageCoords(event->position());

    m_displayPoints.push_back({ imgCoords, isPositive });
    update();

    emit pointAdded(imgCoords.x(), imgCoords.y(), isPositive);
    event->accept();
}

void SegmentCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        m_dragCurrItem = event->position();
        update();
        event->accept();
    } else {
        QQuickPaintedItem::mouseMoveEvent(event);
    }
}

void SegmentCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_isDragging) { event->ignore(); return; }
    m_isDragging = false;

    const QPointF p1 = toImageCoords(m_dragStartItem);
    const QPointF p2 = toImageCoords(m_dragCurrItem);

    // Discard tiny boxes
    if (qAbs(p1.x() - p2.x()) < 2.0 || qAbs(p1.y() - p2.y()) < 2.0) {
        update();
        event->accept();
        return;
    }

    // Normalise to top-left / bottom-right
    const double x1 = qMin(p1.x(), p2.x());
    const double y1 = qMin(p1.y(), p2.y());
    const double x2 = qMax(p1.x(), p2.x());
    const double y2 = qMax(p1.y(), p2.y());

    m_displayBox = { QPointF(x1, y1), QPointF(x2, y2), true };
    update();
    emit boxSet(x1, y1, x2, y2);
    event->accept();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hover
// ─────────────────────────────────────────────────────────────────────────────

void SegmentCanvas::hoverMoveEvent(QHoverEvent *event)
{
    m_cursorPos  = event->position();
    m_showCursor = isInsideImage(m_cursorPos);
    update();
    QQuickPaintedItem::hoverMoveEvent(event);
}

void SegmentCanvas::hoverLeaveEvent(QHoverEvent *event)
{
    m_showCursor = false;
    update();
    QQuickPaintedItem::hoverLeaveEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
//  paint
// ─────────────────────────────────────────────────────────────────────────────

void SegmentCanvas::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), QColor(30, 30, 30));

    if (m_baseImage.isNull()) {
        painter->setPen(QColor(90, 90, 90));
        painter->setFont(QFont("Segoe UI", 11));
        painter->drawText(boundingRect(), Qt::AlignCenter,
                          "Capture a video frame\nor open an image file");
        return;
    }

    const QRectF r = imageRect();
    const double scaleX = r.width()  / m_baseImage.width();
    const double scaleY = r.height() / m_baseImage.height();

    // 1. Source image
    painter->drawImage(r, m_baseImage);

    // 2. SAM mask overlay (blue tint)
    if (!m_maskOverlay.isNull()) {
        painter->setOpacity(m_maskOpacity);
        painter->drawImage(r, m_maskOverlay);
        painter->setOpacity(1.0);
    }

    // 3. Box overlay
    if (m_isDragging) {
        // Real-time drag: semi-transparent fill + dashed border
        const QRectF drag = QRectF(m_dragStartItem, m_dragCurrItem).normalized();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(50, 130, 230, 40));
        painter->drawRect(drag);

        QPen dashPen(QColor(255, 255, 255, 200), 2.0);
        dashPen.setStyle(Qt::DashLine);
        painter->setPen(dashPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(drag);
    } else if (m_displayBox.valid) {
        // Confirmed box: solid border + corner circles
        const double bx1 = r.x() + m_displayBox.imgP1.x() * scaleX;
        const double by1 = r.y() + m_displayBox.imgP1.y() * scaleY;
        const double bx2 = r.x() + m_displayBox.imgP2.x() * scaleX;
        const double by2 = r.y() + m_displayBox.imgP2.y() * scaleY;
        const QRectF boxRect(QPointF(bx1, by1), QPointF(bx2, by2));

        painter->setPen(QPen(QColor(50, 130, 230, 220), 2.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boxRect);

        painter->setPen(QPen(Qt::white, 1.5));
        painter->setBrush(Qt::white);
        constexpr double cr = 4.0;
        painter->drawEllipse(boxRect.topLeft(),     cr, cr);
        painter->drawEllipse(boxRect.topRight(),    cr, cr);
        painter->drawEllipse(boxRect.bottomLeft(),  cr, cr);
        painter->drawEllipse(boxRect.bottomRight(), cr, cr);
    }

    // 4. Point markers
    if (!m_displayPoints.empty()) {
        for (const auto &pt : m_displayPoints) {
            const double sx = r.x() + pt.imageCoords.x() * scaleX;
            const double sy = r.y() + pt.imageCoords.y() * scaleY;

            // Outer white ring
            painter->setPen(QPen(Qt::white, 2.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(QPointF(sx, sy), 7.0, 7.0);

            // Filled circle: green=positive, red=negative
            painter->setPen(Qt::NoPen);
            painter->setBrush(pt.isPositive ? QColor(60, 200, 80) : QColor(220, 60, 60));
            painter->drawEllipse(QPointF(sx, sy), 5.0, 5.0);
        }
    }

    // 5. Cursor crosshair
    if (m_showCursor) {
        const double cx = m_cursorPos.x();
        const double cy = m_cursorPos.y();
        const double arm = 8.0;

        QPen cursorPen(QColor(255, 255, 255, 200), 1.5);
        painter->setPen(cursorPen);
        painter->drawLine(QPointF(cx - arm, cy), QPointF(cx + arm, cy));
        painter->drawLine(QPointF(cx, cy - arm), QPointF(cx, cy + arm));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  QML invokable
// ─────────────────────────────────────────────────────────────────────────────

void SegmentCanvas::setBaseImage(const QString &base64)
{
    QByteArray ba = QByteArray::fromBase64(base64.toUtf8());
    QImage img;
    if (!img.loadFromData(ba)) return;

    m_baseImage = img.convertToFormat(QImage::Format_RGB888);
    m_maskOverlay = QImage{};
    m_displayPoints.clear();
    m_displayBox.valid = false;
    m_isDragging = false;

    emit hasImageChanged();
    emit hasMaskChanged();
    update();
}

void SegmentCanvas::setMaskOverlay(const QString &maskBase64)
{
    if (maskBase64.isEmpty()) {
        m_maskOverlay = QImage{};
        emit hasMaskChanged();
        update();
        return;
    }

    QByteArray ba = QByteArray::fromBase64(maskBase64.toUtf8());
    QImage gray;
    if (!gray.loadFromData(ba)) return;
    gray = gray.convertToFormat(QImage::Format_Grayscale8);

    // Convert Grayscale8 → ARGB32 blue tint overlay
    m_maskOverlay = QImage(gray.size(), QImage::Format_ARGB32);
    m_maskOverlay.fill(Qt::transparent);

    for (int y = 0; y < gray.height(); ++y) {
        const uchar *src = gray.constScanLine(y);
        QRgb        *dst = reinterpret_cast<QRgb *>(m_maskOverlay.scanLine(y));
        for (int x = 0; x < gray.width(); ++x)
            dst[x] = (src[x] > 0) ? qRgba(50, 130, 230, 190) : qRgba(0, 0, 0, 0);
    }

    emit hasMaskChanged();
    update();
}

void SegmentCanvas::undoLastPoint()
{
    if (m_displayPoints.empty()) return;
    m_displayPoints.pop_back();
    update();
}

void SegmentCanvas::clear()
{
    m_displayPoints.clear();
    m_displayBox.valid = false;
    m_isDragging = false;
    m_maskOverlay = QImage{};
    emit hasMaskChanged();
    update();
}

void SegmentCanvas::clearBox()
{
    m_displayBox.valid = false;
    m_isDragging = false;
    update();
}

QString SegmentCanvas::getImageBase64() const
{
    if (m_baseImage.isNull()) return {};

    QByteArray ba;
    QBuffer    buf(&ba);
    buf.open(QIODevice::WriteOnly);
    m_baseImage.save(&buf, "PNG");
    return QString::fromLatin1(ba.toBase64());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Property setter
// ─────────────────────────────────────────────────────────────────────────────

void SegmentCanvas::setMaskOpacity(qreal opacity)
{
    opacity = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_maskOpacity, opacity)) return;
    m_maskOpacity = opacity;
    emit maskOpacityChanged();
    update();
}

void SegmentCanvas::setBoxMode(bool mode)
{
    if (m_boxMode == mode) return;
    m_boxMode = mode;
    if (m_isDragging) {
        m_isDragging = false;
        update();
    }
    emit boxModeChanged();
}
