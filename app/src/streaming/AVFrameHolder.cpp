//
//  DiscoverManager.cpp
//  Moonlight
//
// Created by XITRIX on 31.03.2025.
//

#include "AVFrameHolder.hpp"

#include <algorithm>

namespace {

constexpr size_t kBurstHeadroomFrames = 5;
constexpr auto kArrivalRateWindow = std::chrono::milliseconds(250);
constexpr auto kArrivalRateResetGap = std::chrono::milliseconds(500);
constexpr double kArrivalRateSmoothing = 0.35;
constexpr double kOccupancyCorrectionPerFrame = 0.01;
constexpr double kMaximumOccupancyCorrection = 0.08;

void freeFrameQueue(std::queue<AVFrame*>& frames) {
    for (; !frames.empty(); frames.pop()) {
        AVFrame* frame = frames.front();
        av_frame_free(&frame);
    }
}

void recycleFrame(std::queue<AVFrame*>& freeQueue, AVFrame*& frame) {
    if (!frame) {
        return;
    }

    av_frame_unref(frame);
    freeQueue.push(frame);
    frame = nullptr;
}

} // namespace

AVFrameQueue::AVFrameQueue() {}

AVFrameQueue::~AVFrameQueue() {
    cleanup();
    freeFrameQueue(freeQueue);
}

size_t AVFrameQueue::capacityFor(size_t configuredQueueSize) {
    return std::max<size_t>(configuredQueueSize, 1) + kBurstHeadroomFrames;
}

bool AVFrameQueue::push(AVFrame* item) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!item) {
        return false;
    }

    if (transferOwnership) {
        return pushTransferredLocked(item);
    }

    AVFrame* queuedFrame = acquireFrameLocked();
    if (!queuedFrame) {
        return false;
    }

    if (av_frame_ref(queuedFrame, item) < 0) {
        av_frame_free(&queuedFrame);
        return false;
    }

    queue.push(queuedFrame);
    recordArrivalLocked(std::chrono::steady_clock::now());
    pushesSincePop++;
    maxPushBurstStat = std::max(maxPushBurstStat, pushesSincePop);

    if (queue.size() > limit) {
        const size_t keepFrames = targetBufferedFrames + 1;
        while (queue.size() > keepFrames) {
            AVFrame* droppedFrame = queue.front();
            queue.pop();
            recycleFrame(freeQueue, droppedFrame);
            framesDroppedStat++;
            overflowDropStat++;
        }
        resetArrivalRateEstimatorLocked();
        playoutResyncNeeded = true;
    }

    return true;
}

bool AVFrameQueue::pushTransferred(AVFrame* item) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return pushTransferredLocked(item);
}

AVFrame* AVFrameQueue::pop(bool* consumed) {
    std::lock_guard<std::mutex> lock(m_mutex);

    pushesSincePop = 0;

    if (consumed) {
        *consumed = false;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!drawClockStarted) {
        lastDraw = now;
        drawClockStarted = true;
    } else {
        const auto drawInterval =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastDraw);
        lastDraw = now;

        // App suspension and debugger pauses must not turn into a large burst
        // of overdue video frames when drawing resumes.
        if (drawInterval <= std::chrono::nanoseconds::zero() ||
            drawInterval > std::chrono::milliseconds(250)) {
            averageDrawInterval = std::chrono::nanoseconds::zero();
            frameCredit = 0.0;
            playoutResyncNeeded = true;
        } else if (averageDrawInterval == std::chrono::nanoseconds::zero()) {
            averageDrawInterval = drawInterval;
        } else {
            averageDrawInterval =
                (averageDrawInterval * 15 + drawInterval) / 16;
        }
    }

    if (startupBuffering && queue.size() <= targetBufferedFrames) {
        if (bufferFrame) {
            fakeFrameUsedStat++;
            rebufferHoldStat++;
        }
        return bufferFrame;
    }

    if (startupBuffering) {
        // Establish a small jitter reserve once at startup. Rebuilding the
        // whole reserve after every ordinary miss batches a variable-rate
        // source into visible freeze-and-catch-up cycles.
        startupBuffering = false;
        frameCredit = 0.0;
        playoutResyncNeeded = true;
    }

    size_t dueFrames = 0;
    const bool backlogResync = limit > 0 && queue.size() >= limit;
    if ((playoutResyncNeeded || backlogResync) && !queue.empty()) {
        // Resume immediately after a real miss. If latency has reached the
        // hard limit, discard the stale backlog once instead of repeatedly
        // overflowing the oldest frame while playback remains frozen.
        trimToPlayoutWindowLocked();
        if (backlogResync) {
            resetArrivalRateEstimatorLocked();
        }
        playoutResyncNeeded = false;
        frameCredit = 0.0;
        playoutResyncStat++;
        dueFrames = 1;
    } else if (arrivalRateSamples == 0 ||
               adaptiveFrameInterval <= std::chrono::nanoseconds::zero() ||
               averageDrawInterval <= std::chrono::nanoseconds::zero()) {
        // During the short measurement warm-up, consume only above the jitter
        // reserve. This follows arrivals without assuming configured FPS is
        // the FPS the host is actually producing.
        dueFrames = queue.size() > targetBufferedFrames ? 1 : 0;
    } else if (adaptiveFrameInterval > std::chrono::nanoseconds::zero() &&
               averageDrawInterval > std::chrono::nanoseconds::zero()) {
        const double baseFramesPerDraw =
            static_cast<double>(averageDrawInterval.count()) /
            static_cast<double>(adaptiveFrameInterval.count());
        const double desiredDepth =
            static_cast<double>(targetBufferedFrames + 1);
        const double depthError =
            static_cast<double>(queue.size()) - desiredDepth;
        const double occupancyCorrection = std::clamp(
            depthError * kOccupancyCorrectionPerFrame,
            -kMaximumOccupancyCorrection, kMaximumOccupancyCorrection);
        const double framesPerDraw =
            std::max(0.0, baseFramesPerDraw + occupancyCorrection);

        if (baseFramesPerDraw >= 0.98 && baseFramesPerDraw <= 1.02 &&
            depthError == 0.0) {
            frameCredit = 0.0;
            dueFrames = 1;
        } else {
            frameCredit += framesPerDraw;
            const size_t wholeFrames = static_cast<size_t>(frameCredit);
            frameCredit -= static_cast<double>(wholeFrames);
            dueFrames = std::min(wholeFrames, limit);
        }
    }

    if (dueFrames == 0) {
        scheduledHoldStat++;
        return bufferFrame;
    }

    if (queue.empty()) {
        if (bufferFrame) {
            fakeFrameUsedStat++;
            emptyQueueStat++;
        }
        frameCredit = 0.0;
        playoutResyncNeeded = true;
        // The measured cadence may now be too high because the host FPS fell.
        // Relearn it from fresh arrivals while occupancy pacing protects the
        // jitter reserve, rather than causing repeated underflow/resume cycles.
        resetArrivalRateEstimatorLocked();
        return bufferFrame;
    }

    const size_t consumeCount = std::min(queue.size(), dueFrames);

    recycleFrame(freeQueue, bufferFrame);
    for (size_t i = 0; i < consumeCount; i++) {
        AVFrame* item = queue.front();
        queue.pop();

        if (i + 1 < consumeCount) {
            recycleFrame(freeQueue, item);
            framesDroppedStat++;
            pacingSkipStat++;
        } else {
            bufferFrame = item;
        }
    }

    if (consumed) {
        *consumed = true;
    }
    localClockPacedFrameStat++;

    return bufferFrame;
}

AVFrame* AVFrameQueue::acquireWriteFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return acquireFrameLocked();
}

void AVFrameQueue::recycleWriteFrame(AVFrame*& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    recycleFrame(freeQueue, frame);
}

void AVFrameQueue::configure(size_t queueLimit, int configuredStreamFps,
                             bool transferOwnershipEnabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const size_t configuredDepth = std::max<size_t>(queueLimit, 1);
    limit = capacityFor(configuredDepth);
    targetBufferedFrames =
        configuredDepth > 1 ? std::min<size_t>(configuredDepth - 1, 2) : 0;
    transferOwnership = transferOwnershipEnabled;
    streamFps = configuredStreamFps;
    adaptiveFrameInterval = std::chrono::nanoseconds::zero();
    drawClockStarted = false;
    averageDrawInterval = std::chrono::nanoseconds::zero();
    arrivalClockStarted = false;
    arrivalWindowFrames = 0;
    arrivalRateSamples = 0;
    estimatedSourceFps = 0.0;
    frameCredit = 0.0;
    startupBuffering = true;
    playoutResyncNeeded = true;
}

size_t AVFrameQueue::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return queue.size();
}

size_t AVFrameQueue::targetDepth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return targetBufferedFrames;
}

size_t AVFrameQueue::capacity() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return limit;
}

size_t AVFrameQueue::getFakeFrameUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return fakeFrameUsedStat;
}

size_t AVFrameQueue::getFramesDropStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return framesDroppedStat;
}

size_t AVFrameQueue::getEmptyQueueStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return emptyQueueStat;
}

size_t AVFrameQueue::getRebufferHoldStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return rebufferHoldStat;
}

size_t AVFrameQueue::getOverflowDropStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return overflowDropStat;
}

size_t AVFrameQueue::getPacingSkipStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return pacingSkipStat;
}

size_t AVFrameQueue::getScheduledHoldStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return scheduledHoldStat;
}

size_t AVFrameQueue::getMaxPushBurstStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return maxPushBurstStat;
}

size_t AVFrameQueue::getLocalClockPacedFrameStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return localClockPacedFrameStat;
}

size_t AVFrameQueue::getPlayoutResyncStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return playoutResyncStat;
}

double AVFrameQueue::getEstimatedSourceFps() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return estimatedSourceFps;
}

AVFrame* AVFrameQueue::acquireFrameLocked() {
    AVFrame* frame = nullptr;
    if (!freeQueue.empty()) {
        frame = freeQueue.front();
        freeQueue.pop();
        av_frame_unref(frame);
        return frame;
    }

    frame = av_frame_alloc();
    return frame;
}

bool AVFrameQueue::pushTransferredLocked(AVFrame* item) {
    if (!item) {
        return false;
    }

    queue.push(item);
    recordArrivalLocked(std::chrono::steady_clock::now());
    pushesSincePop++;
    maxPushBurstStat = std::max(maxPushBurstStat, pushesSincePop);

    if (queue.size() > limit) {
        const size_t keepFrames = targetBufferedFrames + 1;
        while (queue.size() > keepFrames) {
            AVFrame* droppedFrame = queue.front();
            queue.pop();
            recycleFrame(freeQueue, droppedFrame);
            framesDroppedStat++;
            overflowDropStat++;
        }
        resetArrivalRateEstimatorLocked();
        playoutResyncNeeded = true;
    }

    return true;
}

void AVFrameQueue::recordArrivalLocked(
    std::chrono::steady_clock::time_point now) {
    if (!arrivalClockStarted) {
        arrivalClockStarted = true;
        arrivalWindowStart = now;
        lastArrival = now;
        arrivalWindowFrames = 1;
        return;
    }

    if (now - lastArrival > kArrivalRateResetGap) {
        // Do not interpret a network pause or app suspension as a permanent
        // low source rate. Start a fresh window when frames resume.
        arrivalWindowStart = now;
        lastArrival = now;
        arrivalWindowFrames = 1;
        return;
    }

    lastArrival = now;
    arrivalWindowFrames++;
    const auto elapsed = now - arrivalWindowStart;
    if (elapsed < kArrivalRateWindow) {
        return;
    }

    if (arrivalWindowFrames > 1) {
        const double elapsedSeconds =
            std::chrono::duration<double>(elapsed).count();
        double sampleFps =
            static_cast<double>(arrivalWindowFrames - 1) / elapsedSeconds;
        const double maximumFps =
            streamFps > 0 ? static_cast<double>(streamFps) : 240.0;
        sampleFps = std::clamp(sampleFps, 1.0, maximumFps);

        if (arrivalRateSamples == 0) {
            estimatedSourceFps = sampleFps;
        } else {
            // The quarter-second sample ignores short decoder bursts. The EMA
            // follows sustained FPS changes without making network jitter a
            // new presentation cadence every window.
            estimatedSourceFps =
                estimatedSourceFps * (1.0 - kArrivalRateSmoothing) +
                sampleFps * kArrivalRateSmoothing;
        }
        arrivalRateSamples++;
        adaptiveFrameInterval = std::chrono::nanoseconds(
            static_cast<int64_t>(1000000000.0 / estimatedSourceFps));
    }

    arrivalWindowStart = now;
    arrivalWindowFrames = 1;
}

void AVFrameQueue::resetArrivalRateEstimatorLocked() {
    arrivalClockStarted = false;
    arrivalWindowFrames = 0;
    arrivalRateSamples = 0;
    estimatedSourceFps = 0.0;
    adaptiveFrameInterval = std::chrono::nanoseconds::zero();
}

void AVFrameQueue::trimToPlayoutWindowLocked() {
    const size_t keepFrames = targetBufferedFrames + 1;
    while (queue.size() > keepFrames) {
        AVFrame* droppedFrame = queue.front();
        queue.pop();
        recycleFrame(freeQueue, droppedFrame);
        framesDroppedStat++;
        pacingSkipStat++;
    }
}

void AVFrameQueue::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);
    fakeFrameUsedStat = 0;
    framesDroppedStat = 0;
    emptyQueueStat = 0;
    rebufferHoldStat = 0;
    overflowDropStat = 0;
    pacingSkipStat = 0;
    scheduledHoldStat = 0;
    pushesSincePop = 0;
    maxPushBurstStat = 0;
    localClockPacedFrameStat = 0;
    playoutResyncStat = 0;
    drawClockStarted = false;
    averageDrawInterval = std::chrono::nanoseconds::zero();
    arrivalClockStarted = false;
    arrivalWindowFrames = 0;
    arrivalRateSamples = 0;
    estimatedSourceFps = 0.0;
    adaptiveFrameInterval = std::chrono::nanoseconds::zero();
    frameCredit = 0.0;
    startupBuffering = true;
    playoutResyncNeeded = true;

    if (bufferFrame) {
        av_frame_free(&bufferFrame);
    }

    freeFrameQueue(queue);
    freeFrameQueue(freeQueue);
    queue = {};
    freeQueue = {};
}
