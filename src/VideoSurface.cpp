#include "VideoSurface.h"
#include "VideoPlayer.h"

#include <QPainter>
#include <QBuffer>

VideoSurface::VideoSurface(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);

    m_timer = new QTimer(this);
    m_timer->setInterval(16);  // ~60fps polling
    connect(m_timer, &QTimer::timeout, this, &VideoSurface::onTimerTick);
}

VideoSurface::~VideoSurface()
{
    stop();
}

bool VideoSurface::open(const QString &url, int w, int h)
{
    stop();

    m_player = std::make_unique<VideoPlayer>();
    if (!m_player->OpenVideo(url, w, h)) {
        m_player.reset();
        setStatusText("Failed to open: " + url);
        return false;
    }

    setStatusText("Playing...");
    setPlaying(true);
    m_lastFrameSeq = 0;
    m_timer->start();
    return true;
}

void VideoSurface::stop()
{
    m_timer->stop();
    if (m_player) {
        m_player->CloseVideo();
        m_player.reset();
    }
    setPlaying(false);
    setStatusText("");
    {
        QMutexLocker lk(&m_mutex);
        m_frame = QImage();
    }
    update();
}

void VideoSurface::onTimerTick()
{
    if (!m_player) return;

    uint64_t seq = m_player->GetFrameSeq();
    if (seq == m_lastFrameSeq) return;
    m_lastFrameSeq = seq;

    QImage frame = m_player->GetCurrentFrame();
    if (frame.isNull()) return;

    {
        QMutexLocker lk(&m_mutex);
        m_frame = std::move(frame);
    }

    double vlcFps = m_player->GetFPS();
    if (qAbs(vlcFps - m_fps) > 0.5) {
        m_fps = vlcFps;
        emit fpsChanged();
    }

    update();
}

void VideoSurface::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::black);

    QImage frame;
    {
        QMutexLocker lk(&m_mutex);
        frame = m_frame;
    }
    if (frame.isNull()) return;

    // Letterbox rendering — preserves aspect ratio with black padding
    QRectF target = boundingRect();
    double scaleX = target.width()  / frame.width();
    double scaleY = target.height() / frame.height();
    double scale  = qMin(scaleX, scaleY);

    double dw = frame.width()  * scale;
    double dh = frame.height() * scale;
    double dx = (target.width()  - dw) / 2.0;
    double dy = (target.height() - dh) / 2.0;

    painter->drawImage(QRectF(dx, dy, dw, dh), frame);
}

QString VideoSurface::grabFrameBase64()
{
    QMutexLocker lk(&m_mutex);
    if (m_frame.isNull()) return {};

    QByteArray ba;
    QBuffer    buf(&ba);
    buf.open(QIODevice::WriteOnly);
    m_frame.save(&buf, "PNG");
    return ba.toBase64();
}

void VideoSurface::setPlaying(bool v)
{
    if (m_playing == v) return;
    m_playing = v;
    emit playingChanged();
}

void VideoSurface::setStatusText(const QString &s)
{
    if (m_statusText == s) return;
    m_statusText = s;
    emit statusTextChanged();
}
