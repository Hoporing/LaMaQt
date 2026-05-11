#pragma once

#include <vlc/vlc.h>
#include <QImage>
#include <mutex>
#include <thread>
#include <memory>
#include <string>
#include <chrono>
#include <queue>
#include <atomic>

enum class ModeVLC
{
    RTSPSTREAM,
    VIDEOFILE,
};

// ============================================================================
//  VLCFrameGrabber  -  LibVLC-based video frame extractor
//
//  Supports local video files (MP4, AVI, MKV, ...) and RTSP streams.
//  Uses the smem (shared memory) output module to intercept decoded RGB frames
//  without rendering them to screen.
//
//  Threading:
//    - VLC callbacks (VideoPrerender / VideoPostrender): called by VLC internally
//    - ProcFrame thread: dequeues frames and outputs at native FPS
//    - CheckFPS thread: measures delivery FPS every 100ms
// ============================================================================
class VLCFrameGrabber
{
public:
    explicit VLCFrameGrabber(ModeVLC mode);
    ~VLCFrameGrabber();

    static ModeVLC GetMode(const std::string &url);

    bool     OpenVideo(const QString &url, int width, int height);

    double   GetFPS()      const;
    void     SetFPS(double fps);
    uint64_t GetFrameSeq() const { return m_frameSeq.load(); }
    int      GetWidth()    const { return m_iWidth; }
    int      GetHeight()   const { return m_iHeight; }

    QImage   GetFrame();
    void     SetFrame(const QImage &frame);

    // VLC smem callbacks (static -> instance dispatch)
    void VideoPrerender(uint8_t **pp_pixel_buffer, int size);
    void VideoPostrender(uint8_t *p_pixel_buffer);

private:
    bool   Init();
    void   PlayMedia();
    void   StopMedia();
    void   ReleaseVLCInstances();

    double GetFPSfromMedia(libvlc_media_t *pMedia);
    void   ProcFrame();   // Frame pacing thread
    void   CheckFPS();    // FPS calculation thread

    static void StaticVideoPrerender(void *p_video_data,
                                     uint8_t **pp_pixel_buffer, int size);
    static void StaticVideoPostrender(void *p_video_data,
                                      uint8_t *p_pixel_buffer);

    // ── Shared libvlc instance ────────────────────────────────────
    static std::shared_ptr<libvlc_instance_t> g_sharedInstance;

    ModeVLC                  m_mode;
    libvlc_instance_t       *m_pInstance     = nullptr;
    libvlc_media_t          *m_pMedia        = nullptr;
    libvlc_media_player_t   *m_pMediaPlayer  = nullptr;
    libvlc_event_manager_t  *m_pEventManager = nullptr;

    // ── Raw buffer + current frame ───────────────────────────────
    std::mutex               m_bufferMutex;
    int                      m_iWidth         = 0;
    int                      m_iHeight        = 0;
    size_t                   m_videoBufferSize = 0;
    int                      m_lastFrameSize  = 0;
    std::unique_ptr<uint8_t[]> m_pVideoBuffer;
    QImage                   m_frame;

    // ── Frame queue (PostRender -> ProcFrame) ────────────────────
    std::mutex               m_mutexQueue;
    std::queue<QImage>       m_bufferQueue;
    bool                     m_bLifeQueue     = false;
    std::thread             *m_pThreadQueue   = nullptr;

    // ── FPS calculation ──────────────────────────────────────────
    mutable std::mutex       m_mutexFPS;
    bool                     m_bLifeFPS       = false;
    std::thread             *m_pThreadFPS     = nullptr;
    std::atomic<int>         m_frameCount     {0};
    std::atomic<uint64_t>    m_frameSeq       {0};
    double                   m_mediaFPS       = 30.0;
    double                   m_fps            = 0.0;
};
