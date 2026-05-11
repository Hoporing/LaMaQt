#include "VLCFrameGrabber.h"

#include <QUrl>
#include <QString>
#include <cstdio>
#include <cstring>
#include <iostream>

// ── Shared libvlc instance ────────────────────────────────────────────────────
std::shared_ptr<libvlc_instance_t> VLCFrameGrabber::g_sharedInstance = nullptr;

// ── Static callbacks → instance dispatch ─────────────────────────────────────

void VLCFrameGrabber::StaticVideoPrerender(void *p_video_data,
                                           uint8_t **pp_pixel_buffer,
                                           int size)
{
    if (auto *self = static_cast<VLCFrameGrabber *>(p_video_data))
        self->VideoPrerender(pp_pixel_buffer, size);
}

void VLCFrameGrabber::StaticVideoPostrender(void *p_video_data,
                                            uint8_t *p_pixel_buffer)
{
    if (auto *self = static_cast<VLCFrameGrabber *>(p_video_data))
        self->VideoPostrender(p_pixel_buffer);
}

// ── RTSP / file mode detection ────────────────────────────────────────────────

ModeVLC VLCFrameGrabber::GetMode(const std::string &url)
{
    if (url.rfind("rtsp://", 0) == 0) return ModeVLC::RTSPSTREAM;
    return ModeVLC::VIDEOFILE;
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

VLCFrameGrabber::VLCFrameGrabber(ModeVLC mode)
    : m_mode(mode)
{
    m_videoBufferSize = 0;
    m_pThreadQueue    = nullptr;
    m_bLifeQueue      = false;
    m_pThreadFPS      = nullptr;
    m_bLifeFPS        = false;
    m_frameCount      = 0;
    m_fps             = 0.0;

    if (!Init()) std::cerr << "[VLC] libvlc init failed\n";
}

VLCFrameGrabber::~VLCFrameGrabber()
{
    StopMedia();
    ReleaseVLCInstances();
}

// ── libvlc initialization ─────────────────────────────────────────────────────

bool VLCFrameGrabber::Init()
{
    if (!g_sharedInstance) {
        const char *args[] = {
            "--ignore-config",
            "--no-video",
            "--drop-late-frames",
            "--skip-frames",
            "--avcodec-hw=none",    // Disable GPU decoding for smem compatibility
            "--network-caching=300",
            "--live-caching=300",
            "--file-caching=300",
        };
        const int argc = static_cast<int>(sizeof(args) / sizeof(args[0]));
        g_sharedInstance = std::shared_ptr<libvlc_instance_t>(
            libvlc_new(argc, args),
            [](libvlc_instance_t *i) { if (i) libvlc_release(i); });

        if (g_sharedInstance) {
            libvlc_log_set(g_sharedInstance.get(),
                [](void *, int level, const libvlc_log_t *, const char *fmt, va_list args) {
                    if (level < LIBVLC_WARNING) return;
                    char buf[512];
                    vsnprintf(buf, sizeof(buf), fmt, args);
                    std::cerr << (level == LIBVLC_ERROR ? "[VLC-ERR] " : "[VLC-WARN]")
                              << " " << buf << "\n";
                }, nullptr);
        }
    }
    m_pInstance = g_sharedInstance.get();
    return m_pInstance != nullptr;
}

// ── OpenVideo ─────────────────────────────────────────────────────────────────

bool VLCFrameGrabber::OpenVideo(const QString &url, int width, int height)
{
    if (!m_pInstance) { std::cerr << "[VLC] FAIL: pInstance is null\n"; return false; }

    m_iWidth  = width;
    m_iHeight = height;

    // ── sout option string ────────────────────────────────────────
    // File:  time-sync + audio enabled
    // RTSP:  no-time-sync (prevents live stream timestamp issues), audio excluded
    char pszOptions[1024] = {};
    if (m_mode == ModeVLC::VIDEOFILE) {
        snprintf(pszOptions, sizeof(pszOptions),
                 ":sout=#transcode{"
                 "vcodec=RV24,"
                 "acodec=s16l,"
                 "threads=2,"
                 "width=%d"
                 "}:smem{"
                 "time-sync,"
                 "video-prerender-callback=%lld,"
                 "video-postrender-callback=%lld,"
                 "video-data=%lld"
                 "}",
                 m_iWidth,
                 reinterpret_cast<long long>(&VLCFrameGrabber::StaticVideoPrerender),
                 reinterpret_cast<long long>(&VLCFrameGrabber::StaticVideoPostrender),
                 reinterpret_cast<long long>(this));
    } else {
        snprintf(pszOptions, sizeof(pszOptions),
                 ":sout=#transcode{"
                 "vcodec=RV24,"
                 "acodec=none,"
                 "threads=2,"
                 "width=%d"
                 "}:smem{"
                 "no-time-sync,"
                 "video-prerender-callback=%lld,"
                 "video-postrender-callback=%lld,"
                 "video-data=%lld"
                 "}",
                 m_iWidth,
                 reinterpret_cast<long long>(&VLCFrameGrabber::StaticVideoPrerender),
                 reinterpret_cast<long long>(&VLCFrameGrabber::StaticVideoPostrender),
                 reinterpret_cast<long long>(this));
    }

    // ── Create media ──────────────────────────────────────────────
    std::string mrl;
    if (m_mode == ModeVLC::VIDEOFILE) {
        mrl = QUrl::fromLocalFile(url).toEncoded().toStdString();
    } else {
        mrl = url.toUtf8().toStdString();
    }

    m_pMedia = libvlc_media_new_location(m_pInstance, mrl.c_str());
    if (!m_pMedia) { std::cerr << "[VLC] FAIL: libvlc_media_new_location\n"; return false; }

    libvlc_media_add_option(m_pMedia, pszOptions);

    if (m_mode == ModeVLC::RTSPSTREAM)
        libvlc_media_add_option(m_pMedia, ":rtsp-tcp");

    // ── Create media player ───────────────────────────────────────
    if (m_pMediaPlayer) libvlc_media_player_release(m_pMediaPlayer);
    m_pMediaPlayer = libvlc_media_player_new_from_media(m_pMedia);
    if (!m_pMediaPlayer) {
        std::cerr << "[VLC] FAIL: libvlc_media_player_new_from_media\n";
        return false;
    }

    m_pEventManager = libvlc_media_player_event_manager(m_pMediaPlayer);

    // Read native FPS (file only; RTSP FPS is unavailable before connection)
    m_mediaFPS = (m_mode == ModeVLC::VIDEOFILE) ? GetFPSfromMedia(m_pMedia) : 30.0;

    PlayMedia();

    // Start frame pacing thread
    m_frameCount = 0;
    m_frameSeq   = 0;
    if (!m_pThreadQueue)
        m_pThreadQueue = new std::thread([this]() { ProcFrame(); });

    // Start FPS calculation thread
    if (!m_pThreadFPS)
        m_pThreadFPS = new std::thread([this]() { CheckFPS(); });

    return true;
}

// ── FPS from media header ──────────────────────────────────────────────────────

double VLCFrameGrabber::GetFPSfromMedia(libvlc_media_t *pMedia)
{
    libvlc_media_parse_with_options(pMedia, libvlc_media_parse_local, 3000);

    for (int i = 0; i < 300; ++i) {
        libvlc_media_parsed_status_t st = libvlc_media_get_parsed_status(pMedia);
        if (st == libvlc_media_parsed_status_done) break;
        if (st == libvlc_media_parsed_status_timeout ||
            st == libvlc_media_parsed_status_failed) {
            std::cerr << "[VLC] Media parse failed/timeout\n";
            return 30.0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    double fps = 0.0;
    libvlc_media_track_t **tracks = nullptr;
    unsigned count = libvlc_media_tracks_get(pMedia, &tracks);
    for (unsigned i = 0; i < count; ++i) {
        if (tracks[i]->i_type == libvlc_track_video) {
            double num = static_cast<double>(tracks[i]->video->i_frame_rate_num);
            double den = static_cast<double>(tracks[i]->video->i_frame_rate_den);
            if (den > 0.0) { fps = num / den; break; }
        }
    }
    if (tracks) libvlc_media_tracks_release(tracks, count);

    return (fps > 0.0) ? fps : 30.0;
}

// ── smem callbacks ────────────────────────────────────────────────────────────

void VLCFrameGrabber::VideoPrerender(uint8_t **pp_pixel_buffer, int size)
{
    if (size > static_cast<int>(m_videoBufferSize) || !m_pVideoBuffer) {
        m_pVideoBuffer.reset(new uint8_t[size]);
        m_videoBufferSize = static_cast<size_t>(size);
    }
    m_lastFrameSize  = size;
    *pp_pixel_buffer = m_pVideoBuffer.get();
}

void VLCFrameGrabber::VideoPostrender(uint8_t * /*p_pixel_buffer*/)
{
    ++m_frameCount;

    int actualH = (m_iWidth > 0 && m_lastFrameSize > 0)
                  ? m_lastFrameSize / (m_iWidth * 3)
                  : m_iHeight;
    QImage img(m_pVideoBuffer.get(), m_iWidth, actualH,
               m_iWidth * 3, QImage::Format_RGB888);

    std::lock_guard<std::mutex> lk(m_mutexQueue);
    while (m_bufferQueue.size() >= 30) m_bufferQueue.pop();
    m_bufferQueue.emplace(img.copy());
}

// ── Frame pacing thread ───────────────────────────────────────────────────────

void VLCFrameGrabber::ProcFrame()
{
    m_bLifeQueue = true;

    double fps = (m_mediaFPS > 0.0) ? m_mediaFPS : 30.0;
    auto frameInterval = std::chrono::microseconds(
        static_cast<int64_t>(1'000'000.0 / fps));

    auto nextTime = std::chrono::steady_clock::now();

    while (m_bLifeQueue) {
        nextTime += frameInterval;  // advance first — no drift even when queue is empty

        QImage frame;
        {
            std::lock_guard<std::mutex> lk(m_mutexQueue);
            if (!m_bufferQueue.empty()) {
                frame = std::move(m_bufferQueue.front());
                m_bufferQueue.pop();
            }
        }

        if (!frame.isNull()) {
            SetFrame(frame);
            ++m_frameSeq;
        }

        std::this_thread::sleep_until(nextTime);
    }
}

// ── FPS calculation thread ────────────────────────────────────────────────────

void VLCFrameGrabber::CheckFPS()
{
    m_bLifeFPS = true;
    auto start = std::chrono::steady_clock::now();

    while (m_bLifeFPS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - start;
        if (elapsed.count() >= 1.0) {
            int count = m_frameCount.exchange(0);
            SetFPS(count / elapsed.count());
            start = now;
        }
    }
}

// ── Frame access ──────────────────────────────────────────────────────────────

QImage VLCFrameGrabber::GetFrame()
{
    std::lock_guard<std::mutex> lk(m_bufferMutex);
    return m_frame;
}

void VLCFrameGrabber::SetFrame(const QImage &frame)
{
    std::lock_guard<std::mutex> lk(m_bufferMutex);
    m_frame = frame;
}

double VLCFrameGrabber::GetFPS() const
{
    std::lock_guard<std::mutex> lk(m_mutexFPS);
    return m_fps;
}

void VLCFrameGrabber::SetFPS(double fps)
{
    std::lock_guard<std::mutex> lk(m_mutexFPS);
    m_fps = fps;
}

// ── Stop / Release ────────────────────────────────────────────────────────────

void VLCFrameGrabber::PlayMedia()
{
    if (m_pMediaPlayer) libvlc_media_player_play(m_pMediaPlayer);
}

void VLCFrameGrabber::StopMedia()
{
    if (m_pMediaPlayer) libvlc_media_player_stop(m_pMediaPlayer);

    if (m_pThreadQueue) {
        m_bLifeQueue = false;
        if (m_pThreadQueue->joinable()) m_pThreadQueue->join();
        delete m_pThreadQueue;
        m_pThreadQueue = nullptr;
    }
    if (m_pThreadFPS) {
        m_bLifeFPS = false;
        if (m_pThreadFPS->joinable()) m_pThreadFPS->join();
        delete m_pThreadFPS;
        m_pThreadFPS = nullptr;
    }

    std::lock_guard<std::mutex> lk(m_mutexQueue);
    while (!m_bufferQueue.empty()) m_bufferQueue.pop();
}

void VLCFrameGrabber::ReleaseVLCInstances()
{
    if (m_pMediaPlayer) {
        libvlc_media_player_stop(m_pMediaPlayer);
        libvlc_media_player_release(m_pMediaPlayer);
        m_pMediaPlayer = nullptr;
    }
    if (m_pMedia) {
        libvlc_media_release(m_pMedia);
        m_pMedia = nullptr;
    }
}
