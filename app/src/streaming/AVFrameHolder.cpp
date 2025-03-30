//
//  DiscoverManager.cpp
//  Moonlight
//
// Created by XITRIX on 31.03.2025.
//

#include "AVFrameHolder.hpp"

AVFrameQueue::AVFrameQueue(size_t limit): limit(limit) {}

AVFrameQueue::~AVFrameQueue() {
    for (; !queue.empty(); queue.pop()) {
        AVFrame* frame = queue.front();
        av_frame_free(&frame);
    }

    for (; !freeQueue.empty(); freeQueue.pop()) {
        AVFrame* frame = freeQueue.front();
        av_frame_free(&frame);
    }
}

void AVFrameQueue::push(AVFrame* item) {
    std::lock_guard<std::mutex> lock(m_mutex);
    queue.push(item);

    if (queue.size() > limit)
        queue.pop();
}

AVFrame* AVFrameQueue::pop() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!queue.empty()) {
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

void AVFrameQueue::cleanup() {
    std::lock_guard<std::mutex> lock(m_mutex);
    fakeFrameUsedStat = 0;
    bufferFrame = nullptr;
    queue = {};
}