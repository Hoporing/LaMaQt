#pragma once

#include "VLCFrameGrabber.h"
#include <QImage>
#include <QString>
#include <memory>

// ============================================================================
//  VideoPlayer  -  Thin abstraction layer over VLCFrameGrabber
// ============================================================================
class VideoPlayer
{
public:
    VideoPlayer();
    ~VideoPlayer();

    bool    OpenVideo(const QString &url, int width = 960, int height = 540);
    void    CloseVideo();

    QImage   GetCurrentFrame();
    double   GetFPS()      const;

    bool     IsPlaying()   const { return m_grabber != nullptr; }
    int      GetWidth()    const { return m_grabber ? m_grabber->GetWidth()    : 0; }
    int      GetHeight()   const { return m_grabber ? m_grabber->GetHeight()   : 0; }
    uint64_t GetFrameSeq() const { return m_grabber ? m_grabber->GetFrameSeq() : 0; }

private:
    std::unique_ptr<VLCFrameGrabber> m_grabber;
};
