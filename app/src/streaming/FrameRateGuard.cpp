#include "FrameRateGuard.hpp"

namespace {

constexpr int kWarmupSeconds = 2;
constexpr int kSustainedSeconds = 3;
constexpr float kShownShare = 0.9f;
constexpr uint32_t kDroppedFrames = 2;
constexpr int kPendingFrames = 2;

uint32_t delta(uint32_t value, uint32_t previous) {
    return value >= previous ? value - previous : value;
}

} // namespace

bool FrameRateGuard::update(const Second& second) {
    const uint32_t drops = delta(second.dropped_frames, m_dropped_frames);
    const uint32_t idrs = delta(second.idr_requests, m_idr_requests);
    m_dropped_frames = second.dropped_frames;
    m_idr_requests = second.idr_requests;

    if (second.fps <= 60 || m_recommended)
        return false;

    if (++m_seconds <= kWarmupSeconds)
        return false;

    // A short second only counts when the console is behind: frames wait
    // for the decoder or get dropped before they are shown. A host that
    // just sends fewer frames isn't a reason to lower the rate.
    const bool backlog = second.pending_frames > kPendingFrames;
    const bool pressure = drops >= kDroppedFrames || idrs > 0 || backlog;
    const bool behind = second.skipped > 0 || backlog;
    if (!fellBehind(second.fps, second.presented) || !pressure || !behind) {
        m_bad_seconds = 0;
        return false;
    }

    if (++m_bad_seconds < kSustainedSeconds)
        return false;

    m_recommended = true;
    return true;
}

void FrameRateGuard::restartWindow() {
    m_seconds = 0;
    m_bad_seconds = 0;
}

bool FrameRateGuard::fellBehind(int fps, uint32_t presented) {
    return fps > 60 && presented < fps * kShownShare;
}
