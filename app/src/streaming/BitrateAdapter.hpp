#pragma once

#include <cstdint>

// Opt-in bitrate step-down for the Automatic bitrate. moonlight-common-c
// fixes the bitrate for a connection, so every change is a reconnect. To
// keep those rare, a launch lowers the bitrate once on a sustained poor
// connection and restores it once after a long stable stretch.
class BitrateAdapter {
  public:
    enum class Change { NONE, LOWER, RESTORE };

    // One second of a connection. The frame counters are cumulative; a value
    // below the previous one is taken as a restarted counter.
    struct Second {
        int bitrate = 0; // kbps of the running connection
        bool poor = false;
        uint32_t rtt_ms = 0; // 0 when unknown
        uint32_t dropped_frames = 0;
        uint32_t idr_requests = 0;
        int pending_frames = 0; // decode units waiting for the decoder
        // Left to the frame-rate guard: a stream above 60 FPS that showed
        // less than 90% of its frames.
        bool fell_behind = false;
    };

    Change update(const Second& second);

    float scale() const { return m_scale; }

    static int scaled_bitrate(int bitrate, float scale);

    // Any reconnect starts a new connection, warm-up included.
    void restartWindow();

  private:
    float m_scale = 1.0f;
    bool m_lowered = false;
    bool m_restored = false;
    int m_seconds = 0;
    uint32_t m_poor_window = 0;
    int m_clean_seconds = 0;
    uint32_t m_dropped_frames = 0;
    uint32_t m_idr_requests = 0;
};
