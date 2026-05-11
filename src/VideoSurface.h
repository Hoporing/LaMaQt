#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QTimer>
#include <QMutex>
#include <memory>

class VideoPlayer;

// ============================================================================
//  VideoSurface  -  QML item for video/RTSP playback (QQuickPaintedItem)
//
//  Polls VideoPlayer at ~60fps, renders frames with letterboxing.
//  Exposes open(url), stop(), grabFrameBase64() to QML.
// ============================================================================
class VideoSurface : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool    playing    READ isPlaying  NOTIFY playingChanged)
    Q_PROPERTY(double  fps        READ fps        NOTIFY fpsChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit VideoSurface(QQuickItem *parent = nullptr);
    ~VideoSurface() override;

    Q_INVOKABLE bool    open(const QString &url, int w = 960, int h = 540);
    Q_INVOKABLE void    stop();

    // Returns current frame as PNG base64 (empty string if no frame available).
    Q_INVOKABLE QString grabFrameBase64();

    bool    isPlaying()  const { return m_playing; }
    double  fps()        const { return m_fps; }
    QString statusText() const { return m_statusText; }

    void paint(QPainter *painter) override;

signals:
    void playingChanged();
    void fpsChanged();
    void statusTextChanged();
    void frameReady();

private slots:
    void onTimerTick();

private:
    void setPlaying(bool v);
    void setStatusText(const QString &s);

    std::unique_ptr<VideoPlayer> m_player;
    QTimer   *m_timer        = nullptr;
    QMutex    m_mutex;
    QImage    m_frame;
    bool      m_playing      = false;
    double    m_fps          = 0.0;
    uint64_t  m_lastFrameSeq = 0;
    QString   m_statusText;
};
