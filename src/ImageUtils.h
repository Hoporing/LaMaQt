#pragma once

#include <QObject>

// ============================================================================
//  ImageUtils  -  C++ image file loading utility for QML
//
//  Exposed as a QML context property ("imageUtils").
//  Use instead of QML Canvas to avoid Canvas texture size limits and
//  main-thread JavaScript stalls when loading large images.
// ============================================================================

class ImageUtils : public QObject
{
    Q_OBJECT

public:
    explicit ImageUtils(QObject *parent = nullptr);

    // Load an image from a file:// URL or a plain filesystem path.
    // If either dimension exceeds maxDim the image is scaled down
    // proportionally (Qt::SmoothTransformation) before encoding.
    // Returns a PNG base64 string, or an empty string on failure.
    Q_INVOKABLE QString loadFile(const QString &fileUrl, int maxDim = 3840) const;
};
