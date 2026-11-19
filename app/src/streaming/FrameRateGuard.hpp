#pragma once

#include <cstdint>

// Falls a stream above 60 FPS back to 60 when the console can't show it.
// After a short warm-up, three seconds in a row that show less than 90% of
// the rate while frames back up or get lost ask for 60, once per launch.
class FrameRateGuard {
  public:
    // One frame-counter second. The lost-frame and keyframe-request counters
    // are cumulative; a value below the previous one is a restarted counter.
    struct Second {
        int fps = 0; // the running connection's rate
        uint32_t presented = 0;
        uint32_t skipped = 0; // decoded frames the queue dropped
        uint32_t dropped_frames = 0;
        uint32_t idr_requests = 0;
        int pending_frames = 0; // decode units waiting for the decoder
    };

    // True once, when the stream should continue at 60 FPS.
    bool update(const Second& second);

    // Any reconnect starts a new connection, warm-up included.
    void restartWindow();

    // A second above 60 FPS that showed less than 90% of the rate.
    static bool fellBehind(int fps, uint32_t presented);

  private:
    bool m_recommended = false;
    int m_seconds = 0;
    int m_bad_seconds = 0;
    uint32_t m_dropped_frames = 0;
    uint32_t m_idr_requests = 0;
};
