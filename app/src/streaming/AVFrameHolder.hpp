#pragma once

#include "Singleton.hpp"
#include <chrono>
#include <functional>
#include "Settings.hpp"
#include <mutex>
#include <queue>

extern "C" {
#include <libavcodec/avcodec.h>
}

class AVFrameQueue {
public:
    explicit AVFrameQueue();
    ~AVFrameQueue();

    bool push(AVFrame* item);
    bool pushTransferred(AVFrame* item);
    AVFrame* pop(bool* consumed = nullptr);
    AVFrame* acquireWriteFrame();
    void recycleWriteFrame(AVFrame*& frame);
    void configure(size_t queueLimit, int streamFps,
                   bool transferOwnershipEnabled);
    static size_t capacityFor(size_t configuredQueueSize);

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t targetDepth() const;
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t getFakeFrameUsage() const;
    [[nodiscard]] size_t getFramesDropStat() const;
    [[nodiscard]] size_t getEmptyQueueStat() const;
    [[nodiscard]] size_t getRebufferHoldStat() const;
    [[nodiscard]] size_t getOverflowDropStat() const;
    [[nodiscard]] size_t getPacingSkipStat() const;
    [[nodiscard]] size_t getMaxPushBurstStat() const;

    void cleanup();

private:
    friend class AVFrameHolder;
    AVFrame* acquireFrameLocked();
    bool pushTransferredLocked(AVFrame* item);
    size_t limit = 0;
    std::queue<AVFrame*> queue;
    std::queue<AVFrame*> freeQueue;
    AVFrame* bufferFrame = nullptr;
    bool transferOwnership = false;
    size_t targetBufferedFrames = 0;
    int streamFps = 0;
    std::chrono::nanoseconds frameInterval{0};
    std::chrono::steady_clock::time_point lastDraw{};
    std::chrono::nanoseconds averageDrawInterval{0};
    double frameCredit = 0.0;
    size_t highOccupancyDraws = 0;
    bool drawClockStarted = false;
    bool buffering = true;
    mutable std::mutex m_mutex;
    size_t fakeFrameUsedStat = 0;
    size_t framesDroppedStat = 0;
    size_t emptyQueueStat = 0;
    size_t rebufferHoldStat = 0;
    size_t overflowDropStat = 0;
    size_t pacingSkipStat = 0;
    size_t pushesSincePop = 0;
    size_t maxPushBurstStat = 0;
};

class AVFrameHolder : public Singleton<AVFrameHolder> {
  public:
    void push(AVFrame* frame) {
        m_frame_queue.push(frame);
    }

    void pushTransferred(AVFrame* frame) {
        m_frame_queue.pushTransferred(frame);
    }

    void get(const std::function<void(AVFrame*)>& fn) {
        auto frame = m_frame_queue.pop();

        if (frame) {
            fn(frame);
        }
    }

    AVFrame* acquireWriteFrame() {
        return m_frame_queue.acquireWriteFrame();
    }

    void recycleWriteFrame(AVFrame*& frame) {
        m_frame_queue.recycleWriteFrame(frame);
    }

    void prepare(int streamFps, bool transferOwnership = false) {
        m_frame_queue.configure(Settings::instance().frames_queue_size(),
                                streamFps, transferOwnership);
    }

    void cleanup() {
        m_frame_queue.cleanup();
    }

    [[nodiscard]] size_t getFakeFrameStat() const { return m_frame_queue.getFakeFrameUsage(); }
    [[nodiscard]] size_t getFrameDropStat() const { return m_frame_queue.getFramesDropStat(); }
    [[nodiscard]] size_t getFrameQueueSize() const { return m_frame_queue.size(); }
    [[nodiscard]] size_t getFrameQueueTargetDepth() const { return m_frame_queue.targetDepth(); }
    [[nodiscard]] size_t getFrameQueueCapacity() const { return m_frame_queue.capacity(); }
    [[nodiscard]] size_t getFrameQueueEmptyStat() const { return m_frame_queue.getEmptyQueueStat(); }
    [[nodiscard]] size_t getFrameQueueRebufferHoldStat() const { return m_frame_queue.getRebufferHoldStat(); }
    [[nodiscard]] size_t getFrameQueueOverflowDropStat() const { return m_frame_queue.getOverflowDropStat(); }
    [[nodiscard]] size_t getFrameQueuePacingSkipStat() const { return m_frame_queue.getPacingSkipStat(); }
    [[nodiscard]] size_t getFrameQueueMaxPushBurstStat() const { return m_frame_queue.getMaxPushBurstStat(); }

  private:
    AVFrameQueue m_frame_queue;
};
