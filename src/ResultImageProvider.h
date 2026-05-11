#pragma once

#include <QQuickImageProvider>
#include <QImage>
#include <QMutex>

// Provides QML with the latest inpainting result via image://result/frame?v=N
class ResultImageProvider : public QQuickImageProvider
{
public:
    explicit ResultImageProvider();

    // Called from LaMaBridge (worker thread) — thread-safe
    void updateResult(const QImage &image);

    int version() const;

    // Called by Qt Quick scene graph — thread-safe
    QImage requestImage(const QString &id,
                        QSize *size,
                        const QSize &requestedSize) override;

private:
    mutable QMutex m_mutex;
    QImage         m_image;
    int            m_version = 0;
};
