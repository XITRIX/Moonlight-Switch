//
// Created by Даниил Виноградов on 23.03.2025.
//


#pragma once
#include <SDL.h>
#include <queue>
#include <deque>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <libavcodec/avcodec.h>
}

class IFFmpegRenderer {
public:
    virtual void renderFrame(AVFrame* frame) = 0;

    virtual bool isRenderThreadSupported() {
        // Render thread is not supported by default
        return false;
    }

    // Called on the same thread as renderFrame() during destruction of the renderer
    virtual void cleanupRenderContext() {
        // Nothing
    }

    // Called for threaded renderers to allow them to wait prior to us latching
    // the next frame for rendering (as opposed to waiting on buffer swap with
    // an older frame already queued for display).
    virtual void waitToRender() {
        // Don't wait by default
    }
};

class IVsyncSource {
public:
    virtual ~IVsyncSource() {}
    virtual bool initialize(SDL_Window* window, int displayFps) = 0;

    // Asynchronous sources produce callbacks on their own, while synchronous
    // sources require calls to waitForVsync().
    virtual bool isAsync() = 0;

    virtual void waitForVsync() {
        // Synchronous sources must implement waitForVsync()!
        SDL_assert(false);
    }
};

class Pacer
{
public:
    Pacer(IFFmpegRenderer* renderer); //, PVIDEO_STATS videoStats);

    ~Pacer();

    void submitFrame(AVFrame* frame);

    bool initialize(SDL_Window* window, int maxVideoFps, bool enablePacing);

    void signalVsync();

    void renderOnMainThread();

private:
    static int vsyncThread(void* context);

    static int renderThread(void* context);

    void handleVsync(int timeUntilNextVsyncMillis);

    void enqueueFrameForRenderingAndUnlock(AVFrame* frame);

    void renderFrame(AVFrame* frame);

    void dropFrameForEnqueue(std::queue<AVFrame*>& queue);

    std::queue<AVFrame*> m_RenderQueue;
    std::queue<AVFrame*> m_PacingQueue;
    std::deque<int> m_PacingQueueHistory;
    std::deque<int> m_RenderQueueHistory;
    std::mutex m_FrameQueueLock;
    std::condition_variable m_RenderQueueNotEmpty;
    std::condition_variable m_PacingQueueNotEmpty;
    std::condition_variable m_VsyncSignalled;
    SDL_Thread* m_RenderThread;
    SDL_Thread* m_VsyncThread;
    bool m_Stopping;

    IVsyncSource* m_VsyncSource;
    IFFmpegRenderer* m_VsyncRenderer;
    int m_MaxVideoFps;
    int m_DisplayFps;
//    PVIDEO_STATS m_VideoStats;
    int m_RendererAttributes;

    static int getDisplayRefreshRate(SDL_Window* window);
};
