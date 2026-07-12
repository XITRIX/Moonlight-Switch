//
//  DiscoverManager.cpp
//  Moonlight
//
// Created by XITRIX on 31.03.2025.
//

#include "AVFrameHolder.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr size_t kBurstHeadroomFrames = 5;

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
    pushesSincePop++;
    maxPushBurstStat = std::max(maxPushBurstStat, pushesSincePop);

    if (queue.size() > limit) {
        AVFrame* droppedFrame = queue.front();
        queue.pop();
        recycleFrame(freeQueue, droppedFrame);
        framesDroppedStat ++;
        overflowDropStat++;
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
    const bool paced = frameInterval.count() > 0;
    size_t dueFrames = 1;
    bool occupancyPaced = false;
    size_t occupancyConsumeLimit = 1;
    if (paced) {
        if (!drawClockStarted) {
            lastDraw = now;
            drawClockStarted = true;
        } else {
            auto drawInterval =
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastDraw);
            lastDraw = now;

            // App suspension and debugger pauses must not turn into a large
            // burst of overdue video frames when drawing resumes.
            if (drawInterval <= std::chrono::nanoseconds::zero() ||
                drawInterval > std::chrono::milliseconds(250)) {
                averageDrawInterval = std::chrono::nanoseconds::zero();
                frameCredit = 0.0;
            } else if (averageDrawInterval == std::chrono::nanoseconds::zero()) {
                averageDrawInterval = drawInterval;
            } else {
                // Smooth UI scheduling jitter before converting display draws
                // into source-frame advances.
                averageDrawInterval =
                    (averageDrawInterval * 15 + drawInterval) / 16;
            }

            if (averageDrawInterval > std::chrono::nanoseconds::zero()) {
                const double framesPerDraw =
                    static_cast<double>(averageDrawInterval.count()) /
                    static_cast<double>(frameInterval.count());

                // When the configured stream rate is substantially above the
                // display rate, actual delivery may still vary below that
                // configured maximum. Follow real queue occupancy instead of
                // blindly consuming the nominal number of source frames.
                if (framesPerDraw > 1.10) {
                    frameCredit = 0.0;
                    dueFrames = 1;
                    occupancyPaced = true;
                    occupancyConsumeLimit = static_cast<size_t>(
                        std::ceil(framesPerDraw));
                }
                // Lock nearly identical stream and display clocks to 1:1.
                // Without this deadband, tiny draw-time jitter repeatedly
                // creates a hold followed by a two-frame catch-up.
                else if (framesPerDraw >= 0.98 && framesPerDraw <= 1.02) {
                    frameCredit = 0.0;
                    dueFrames = 1;
                } else {
                    frameCredit += framesPerDraw;
                    dueFrames = static_cast<size_t>(frameCredit);
                    frameCredit -= static_cast<double>(dueFrames);
                }
            }
        }
    }

    if (buffering && queue.size() <= targetBufferedFrames) {
        if (bufferFrame && dueFrames > 0) {
            fakeFrameUsedStat++;
            rebufferHoldStat++;
        }
        return bufferFrame;
    }

    if (buffering) {
        // Start or resume with the oldest buffered frame and establish the
        // configured jitter reserve before following the regular cadence.
        dueFrames = 1;
        frameCredit = 0.0;
    } else if (dueFrames == 0) {
        return bufferFrame;
    }

    if (occupancyPaced && !buffering) {
        // Treat a high configured FPS as a maximum rather than a promise that
        // every draw has that many new frames. Consume one frame normally and
        // accelerate only after backlog remains near the hard capacity. This
        // preserves burst frames for the delivery gap that usually follows.
        const size_t highWatermark = limit > 1 ? limit - 1 : limit;
        if (queue.size() >= highWatermark) {
            highOccupancyDraws++;
        } else {
            highOccupancyDraws = 0;
        }
        dueFrames = highOccupancyDraws >= 2
                        ? std::min(occupancyConsumeLimit, queue.size())
                        : 1;
    } else {
        highOccupancyDraws = 0;
    }

    if (queue.empty()) {
        if (bufferFrame) {
            fakeFrameUsedStat++;
            emptyQueueStat++;
        }
        buffering = true;
        frameCredit = 0.0;
        return bufferFrame;
    }

    // When rendering falls behind the stream clock, coalesce all overdue
    // frames into this presentation. Transient producer bursts remain queued
    // so they can cover a later arrival gap.
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

    buffering = false;

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
    frameInterval = streamFps > 0
                        ? std::chrono::nanoseconds(1000000000LL / streamFps)
                        : std::chrono::nanoseconds::zero();
    drawClockStarted = false;
    averageDrawInterval = std::chrono::nanoseconds::zero();
    frameCredit = 0.0;
    highOccupancyDraws = 0;
    buffering = true;
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

size_t AVFrameQueue::getMaxPushBurstStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return maxPushBurstStat;
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
    pushesSincePop++;
    maxPushBurstStat = std::max(maxPushBurstStat, pushesSincePop);

    if (queue.size() > limit) {
        AVFrame* droppedFrame = queue.front();
        queue.pop();
        recycleFrame(freeQueue, droppedFrame);
        framesDroppedStat++;
        overflowDropStat++;
    }

    return true;
}

void AVFrameQueue::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);
    fakeFrameUsedStat = 0;
    framesDroppedStat = 0;
    emptyQueueStat = 0;
    rebufferHoldStat = 0;
    overflowDropStat = 0;
    pacingSkipStat = 0;
    pushesSincePop = 0;
    maxPushBurstStat = 0;
    drawClockStarted = false;
    averageDrawInterval = std::chrono::nanoseconds::zero();
    frameCredit = 0.0;
    highOccupancyDraws = 0;
    buffering = true;

    if (bufferFrame) {
        av_frame_free(&bufferFrame);
    }

    freeFrameQueue(queue);
    freeFrameQueue(freeQueue);
    queue = {};
    freeQueue = {};
}
