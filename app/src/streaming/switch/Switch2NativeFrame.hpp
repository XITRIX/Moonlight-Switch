#pragma once
#ifdef __SWITCH2__
#include <memory>
#include <mutex>
#include <new>
#include <vector>
extern "C"
{
#include <libavutil/frame.h>
#include <switch2/video/decoder.h>
}

// A private AVFrame format: opaque_ref owns a decoder slot, rather than pixels.
constexpr int kSwitch2NativeFrame = -0x535732;

struct Switch2DecoderPool
{
    VideoDecoder decoder { };
    bool open = false;
    std::mutex releaseMutex;
    std::vector<VideoFrame> released;

    Switch2DecoderPool() { released.reserve(VIDDEC_MAX_IMAGES); }
    ~Switch2DecoderPool()
    {
        if (open)
            videoDecoderClose(&decoder);
    }
    void retireDecoder()
    {
        // Rendering may retain output images, but must not retain the native
        // decoder session/pixel budget after a stop or failed submission.
        if (decoder.pending_armed)
        {
            gsvSyncDisarmEvent(&decoder.sync, decoder.event_slot);
            decoder.pending_armed = false;
        }
        if (decoder.started)
        {
            viddecDecoderStop(&decoder.decoder);
            decoder.started = false;
        }
        if (decoder.decoder_open)
        {
            viddecDecoderClose(&decoder.decoder);
            decoder.decoder_open = false;
        }
    }
    void deferRelease(const VideoFrame& frame)
    {
        std::lock_guard<std::mutex> lock(releaseMutex);
        released.push_back(frame);
    }
    // Called only by the decoder, under its SDK mutex. AVFrame destruction may
    // happen on either thread or inside the queue lock, so it never calls IPC.
    void releaseFrames()
    {
        std::lock_guard<std::mutex> lock(releaseMutex);
        for (auto& frame : released)
            videoDecoderReleaseFrame(&decoder, &frame);
        released.clear();
    }
};

struct Switch2NativeFrame
{
    std::shared_ptr<Switch2DecoderPool> pool;
    VideoFrame frame;
    ~Switch2NativeFrame()
    {
        if (pool)
            pool->deferRelease(frame);
    }
};

inline Switch2NativeFrame* switch2NativeFrame(const AVFrame* frame)
{
    return frame && frame->format == kSwitch2NativeFrame && frame->opaque_ref
        ? reinterpret_cast<Switch2NativeFrame*>(frame->opaque_ref->data)
        : nullptr;
}

inline bool switch2AttachNativeFrame(
    AVFrame* out, const std::shared_ptr<Switch2DecoderPool>& pool, const VideoFrame& frame)
{
    auto* lease = new (std::nothrow) Switch2NativeFrame { pool, frame };
    if (!lease)
        return false;
    out->opaque_ref = av_buffer_create(
        reinterpret_cast<uint8_t*>(lease), sizeof(*lease), [](void*, uint8_t* data)
        { delete reinterpret_cast<Switch2NativeFrame*>(data); }, nullptr, AV_BUFFER_FLAG_READONLY);
    if (!out->opaque_ref)
    {
        // Failure did not transfer the slot; the caller releases it.
        lease->pool.reset();
        delete lease;
        return false;
    }
    out->format = kSwitch2NativeFrame;
    return true;
}
#endif
