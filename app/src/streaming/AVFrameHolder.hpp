#include "Singleton.hpp"
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
}

#pragma once

class AVFrameHolder : public Singleton<AVFrameHolder> {
  public:
    void push(AVFrame* frame) {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_frame = frame;
    }

    void get(const std::function<void(AVFrame*)> fn) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_frame) {
            fn(m_frame);
        }
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_frame = nullptr;
    }

  private:
    std::mutex m_mutex;
    AVFrame* m_frame = nullptr;
};
