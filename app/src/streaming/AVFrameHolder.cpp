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

    AVFrame* queuedFrame = nullptr;
    if (!freeQueue.empty()) {
        queuedFrame = freeQueue.front();
        freeQueue.pop();
        av_frame_unref(queuedFrame);
    } else {
        queuedFrame = av_frame_alloc();
        if (!queuedFrame) {
            return false;
        }
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

AVFrame* AVFrameQueue::pop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!queue.empty()) {
        recycleFrame(freeQueue, bufferFrame);

        AVFrame* item = queue.front();
        queue.pop();
        bufferFrame = item;
    } else {
        fakeFrameUsedStat ++;
    }

    return bufferFrame;
}

size_t AVFrameQueue::size() const {
    return queue.size();
}

size_t AVFrameQueue::getFakeFrameUsage() const {
    return fakeFrameUsedStat;
}

size_t AVFrameQueue::getFramesDropStat() const {
    return framesDroppedStat;
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
