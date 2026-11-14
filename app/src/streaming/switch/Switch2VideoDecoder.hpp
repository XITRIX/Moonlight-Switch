#pragma once
#ifdef __SWITCH2__

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "IFFmpegVideoDecoder.hpp"
#include "Switch2NativeFrame.hpp"
extern "C"
{
#include <switch2/video/decoder.h>
}

class FFmpegVideoDecoder;

class Switch2VideoDecoder : public IFFmpegVideoDecoder
{
  public:
    ~Switch2VideoDecoder() override;
    int setup(int format, int width, int height, int fps, void*, int) override;
    void start() override;
    void stop() override;
    void cleanup() override;
    int submit_decode_unit(PDECODE_UNIT unit) override;
    int capabilities() const override;
    VideoDecodeStats* video_decode_stats() override;

  private:
    bool openHardware(const std::vector<uint8_t>& config);
    void closeHardware(); // Call with m_mutex held, after stopping the reader on cleanup.
    bool drain(uint64_t timeout); // All SDK access is serialized by m_mutex.
    void readFrames();
    void fail(const char* operation, Result rc);

    std::shared_ptr<Switch2DecoderPool> m_pool;
    bool m_open        = false;
    bool m_need_idr    = true;
    bool m_first_frame = false;
    int m_format = 0, m_width = 0, m_height = 0, m_fps = 0;
    uint32_t m_last_frame  = 0;
    bool m_have_last_frame = false;
    uint64_t m_started_ms = 0, m_window_ms = 0;
    uint64_t m_retry_after_ms = 0;
    std::unique_ptr<FFmpegVideoDecoder> m_software;
    std::atomic<bool> m_stopped { true };
    std::thread m_reader;
    std::mutex m_mutex;
    VideoDecodeStats m_progress { }, m_snapshot { };
    std::deque<std::pair<int64_t, uint64_t>> m_pending_times;
};
#endif
