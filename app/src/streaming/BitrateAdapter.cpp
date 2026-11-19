#include "BitrateAdapter.hpp"

#include <algorithm>
#include <bitset>

namespace {

constexpr int kWarmupSeconds = 5;
constexpr int kWindowSeconds = 10;
constexpr int kPoorSecondsToLower = 6;
constexpr int kCleanSecondsToRestore = 120;
constexpr float kLowerScale = 0.75f;
constexpr int kFloorBitrate = 5000;
constexpr int kBitrateStep = 100;

constexpr uint32_t kPoorDroppedFrames = 5;
constexpr uint32_t kPoorRttMs = 80;
constexpr int kPoorPendingFrames = 3;

uint32_t delta(uint32_t value, uint32_t previous) {
    return value >= previous ? value - previous : value;
}

bool isPoor(const BitrateAdapter::Second& second, uint32_t drops,
            uint32_t idrs) {
    return second.poor || second.rtt_ms > kPoorRttMs ||
           drops >= kPoorDroppedFrames || idrs > 0 ||
           second.pending_frames > kPoorPendingFrames;
}

} // namespace

BitrateAdapter::Change BitrateAdapter::update(const Second& second) {
    const uint32_t drops = delta(second.dropped_frames, m_dropped_frames);
    const uint32_t idrs = delta(second.idr_requests, m_idr_requests);
    m_dropped_frames = second.dropped_frames;
    m_idr_requests = second.idr_requests;

    // Skip the first seconds of each connection, which are dominated by
    // the startup keyframe burst.
    if (++m_seconds <= kWarmupSeconds)
        return Change::NONE;

    const bool poor = isPoor(second, drops, idrs);
    m_poor_window = ((m_poor_window << 1) | (poor ? 1 : 0)) &
                    ((1u << kWindowSeconds) - 1);
    m_clean_seconds = poor ? 0 : m_clean_seconds + 1;

    // At the floor a lower bitrate would reconnect at the same rate.
    if (!m_lowered && second.bitrate > kFloorBitrate &&
        (int)std::bitset<kWindowSeconds>(m_poor_window).count() >=
            kPoorSecondsToLower) {
        m_lowered = true;
        m_scale = kLowerScale;
        restartWindow();
        return Change::LOWER;
    }

    if (m_lowered && !m_restored && m_clean_seconds >= kCleanSecondsToRestore) {
        m_restored = true;
        m_scale = 1.0f;
        restartWindow();
        return Change::RESTORE;
    }

    return Change::NONE;
}

int BitrateAdapter::scaled_bitrate(int bitrate, float scale) {
    if (scale >= 1.0f)
        return bitrate;

    const int lowered = (int)(bitrate * scale) / kBitrateStep * kBitrateStep;
    return std::max(lowered, std::min(bitrate, kFloorBitrate));
}

void BitrateAdapter::restartWindow() {
    m_seconds = 0;
    m_poor_window = 0;
    m_clean_seconds = 0;
}
