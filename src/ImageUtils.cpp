#include "ImageUtils.h"

#include <QImage>
#include <QUrl>
#include <QByteArray>
#include <QBuffer>

ImageUtils::ImageUtils(QObject *parent) : QObject(parent) {}

QString ImageUtils::loadFile(const QString &fileUrl, int maxDim) const
{
    // Accept both file:// URLs and plain filesystem paths
    const QString path = fileUrl.startsWith("file:")
                         ? QUrl(fileUrl).toLocalFile()
                         : fileUrl;

    QImage img;
    if (!img.load(path)) return {};

    // Scale down if either dimension exceeds maxDim (preserve aspect ratio)
    if (img.width() > maxDim || img.height() > maxDim) {
        img = img.scaled(maxDim, maxDim,
                         Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    }

    QByteArray ba;
    QBuffer    buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return QString::fromLatin1(ba.toBase64());
}
