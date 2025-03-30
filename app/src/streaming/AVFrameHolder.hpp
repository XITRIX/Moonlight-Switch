#pragma once

#include "Singleton.hpp"
#include <mutex>
#include <functional>
#include <queue>

extern "C" {
#include <libavcodec/avcodec.h>
}

#define m_frames_count 4

class AVFrameQueue {
public:
    explicit AVFrameQueue(size_t limit = m_frames_count - 1);
    ~AVFrameQueue();

    void push(AVFrame* item);
    AVFrame* pop();

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t getFakeFrameUsage() const;

    void cleanup();

private:
    size_t limit;
    std::queue<AVFrame*> queue;
    std::queue<AVFrame*> freeQueue;
    AVFrame* bufferFrame = nullptr;
    std::mutex m_mutex;
    size_t fakeFrameUsedStat = 0;
};

class AVFrameHolder : public Singleton<AVFrameHolder> {
  public:
    void push(AVFrame* frame) {
        m_frame_queue.push(frame);
        stat ++;
    }

    void get(const std::function<void(AVFrame*)>& fn) {
        auto frame = m_frame_queue.pop();

        if (frame) {
            fn(frame);
            stat --;
        }
    }

    void cleanup() {
        m_frame_queue.cleanup();
        stat = 0;
    }

    [[nodiscard]] int getStat() const { return stat; }
    [[nodiscard]] size_t getFakeFrameStat() const { return m_frame_queue.getFakeFrameUsage(); }
    [[nodiscard]] size_t getFrameQueueSize() const { return m_frame_queue.size(); }

  private:
    AVFrameQueue m_frame_queue;
    int stat = 0;
};
