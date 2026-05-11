#include "ResultImageProvider.h"

ResultImageProvider::ResultImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

void ResultImageProvider::updateResult(const QImage &image)
{
    QMutexLocker lk(&m_mutex);
    m_image = image;
    ++m_version;
}

int ResultImageProvider::version() const
{
    QMutexLocker lk(&m_mutex);
    return m_version;
}

QImage ResultImageProvider::requestImage(const QString & /*id*/,
                                         QSize *size,
                                         const QSize &requestedSize)
{
    QMutexLocker lk(&m_mutex);
    if (size) *size = m_image.size();
    if (!requestedSize.isEmpty() && !m_image.isNull())
        return m_image.scaled(requestedSize, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation);
    return m_image;
}
