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

} // namespace

BitrateAdapter::Change BitrateAdapter::update(bool poor) {
    // Skip the first seconds of each connection, which are dominated by
    // the startup keyframe burst.
    if (++m_seconds <= kWarmupSeconds)
        return Change::NONE;

    m_poor_window = ((m_poor_window << 1) | (poor ? 1 : 0)) &
                    ((1u << kWindowSeconds) - 1);
    m_clean_seconds = poor ? 0 : m_clean_seconds + 1;

    if (!m_lowered &&
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
