#pragma once
#include <memory>
#include <new>
extern "C"
{
#include <libavutil/buffer.h>
}

inline std::shared_ptr<void> switch2FrameReference(AVBufferRef* frame)
{
    auto* ref = av_buffer_ref(frame);
    if (!ref)
        throw std::bad_alloc();
    return { ref, [](void* value)
        {
            auto* buffer = static_cast<AVBufferRef*>(value);
            av_buffer_unref(&buffer);
        } };
}
