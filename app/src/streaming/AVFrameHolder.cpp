//
//  DiscoverManager.cpp
//  Moonlight
//
// Created by XITRIX on 31.03.2025.
//

#include "AVFrameHolder.hpp"

namespace {

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

    if (queue.size() > limit) {
        AVFrame* droppedFrame = queue.front();
        queue.pop();
        recycleFrame(freeQueue, droppedFrame);
        framesDroppedStat ++;
    }

    return true;
}

bool AVFrameQueue::pushTransferred(AVFrame* item) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return pushTransferredLocked(item);
}

AVFrame* AVFrameQueue::pop(bool* consumed) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (consumed) {
        *consumed = false;
    }

    if (!queue.empty()) {
        recycleFrame(freeQueue, bufferFrame);

        AVFrame* item = queue.front();
        queue.pop();
        bufferFrame = item;
        if (consumed) {
            *consumed = true;
        }
    } else {
        fakeFrameUsedStat ++;
    }

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

void AVFrameQueue::setTransferOwnership(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    transferOwnership = enabled;
}

size_t AVFrameQueue::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return queue.size();
}

size_t AVFrameQueue::getFakeFrameUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return fakeFrameUsedStat;
}

size_t AVFrameQueue::getFramesDropStat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return framesDroppedStat;
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

    if (queue.size() > limit) {
        AVFrame* droppedFrame = queue.front();
        queue.pop();
        recycleFrame(freeQueue, droppedFrame);
        framesDroppedStat++;
    }

    return true;
}

void AVFrameQueue::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);
    fakeFrameUsedStat = 0;
    framesDroppedStat = 0;

    if (bufferFrame) {
        av_frame_free(&bufferFrame);
    }

    freeFrameQueue(queue);
    freeFrameQueue(freeQueue);
    queue = {};
    freeQueue = {};
}
