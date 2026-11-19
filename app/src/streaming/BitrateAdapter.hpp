#pragma once

#include <cstdint>

// Opt-in bitrate step-down for the Automatic bitrate. moonlight-common-c
// fixes the bitrate for a connection, so every change is a reconnect. To
// keep those rare, a launch lowers the bitrate once on a sustained poor
// connection and restores it once after a long stable stretch.
class BitrateAdapter {
  public:
    enum class Change { NONE, LOWER, RESTORE };

    // Feed one second of streaming and whether the connection was poor.
    Change update(bool poor);

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
};
