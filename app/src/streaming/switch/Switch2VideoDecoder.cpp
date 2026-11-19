#ifdef __SWITCH2__
#include "Switch2VideoDecoder.hpp"

#include <algorithm>
#include <borealis.hpp>
#include <chrono>
#include <limits>

#include "AVFrameHolder.hpp"
#include "FFmpegVideoDecoder.hpp"
#include "Settings.hpp"
#include "Switch2ImageInterop.hpp"

namespace
{
constexpr size_t kMaxSample  = 4 * 1024 * 1024;
constexpr uint64_t kPollNs   = 2000000; // Bounded waits keep stop/join responsive.
constexpr uint64_t kSubmitNs = 50000000; // Allow a large 4K IDR to traverse all SDK input chunks.
bool timedOut(Result rc) { return R_VALUE(rc) == KERNELRESULT(TimedOut); }
}

Switch2VideoDecoder::~Switch2VideoDecoder() { cleanup(); }

int Switch2VideoDecoder::setup(int format, int width, int height, int fps, void* context, int flags)
{
    cleanup();
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || fps <= 0)
        return -1;
    const bool hardware = format & hardwareFormats();
    const bool software = format & softwareFormats();
    if (!hardware && !software)
    {
        brls::Logger::error("Switch 2 decoder: no decoder for format 0x{:x}", format);
        return -1;
    }
    if (!hardware || (software && !Settings::instance().use_hw_decoding()))
    {
        brls::Logger::info("Switch 2 decoder: software format 0x{:x} (hardware available={}, setting={})", format,
            hardware, Settings::instance().use_hw_decoding());
        m_software = std::make_unique<FFmpegVideoDecoder>();
        return m_software->setup(format, width, height, fps, context, flags);
    }
    m_format     = format;
    m_width      = width;
    m_height     = height;
    m_fps        = fps;
    m_progress   = { };
    m_snapshot   = { };
    m_started_ms = m_window_ms             = LiGetMillis();
    m_retry_after_ms                       = 0;
    m_progress.measurement_start_timestamp = m_started_ms;
    m_have_last_frame                      = false;
    m_first_frame                          = false;
    m_need_idr                             = true;
    AVFrameHolder::instance().prepare(fps, true, 4);
    brls::Logger::info("Switch 2 decoder: native {} requested, {}x{} at {} FPS; awaiting keyframe",
        format == VIDEO_FORMAT_H264 ? "H.264" : "AV1", width, height, fps);
    return 0;
}

void Switch2VideoDecoder::start()
{
    if (m_software)
    {
        m_software->start();
        return;
    }
    if (!m_stopped.exchange(false))
        return;
    m_reader = std::thread(&Switch2VideoDecoder::readFrames, this);
}

void Switch2VideoDecoder::stop()
{
    if (m_software)
        m_software->stop();
    m_stopped.store(true);
    if (m_reader.joinable())
        m_reader.join();
}

void Switch2VideoDecoder::cleanup()
{
    stop();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_software)
    {
        m_software->cleanup();
        m_software.reset();
    }
    closeHardware();
    AVFrameHolder::instance().cleanup();
    m_need_idr = true;
}

void Switch2VideoDecoder::closeHardware()
{
    if (m_pool && m_pool->open)
        m_pool->retireDecoder();
    m_open = false;
    // The last frame/import releases the pool after GPU completion.
    m_pool.reset();
    m_pending_times.clear();
}

bool Switch2VideoDecoder::openHardware(const std::vector<uint8_t>& config)
{
    struct ImportScope
    {
        explicit ImportScope(bool enable) { switch2PreimportDecoderImages = enable; }
        ~ImportScope() { switch2PreimportDecoderImages = false; }
    } importScope(m_format == VIDEO_FORMAT_H264);
    closeHardware();
    m_pool = std::make_shared<Switch2DecoderPool>();
    VideoDecoderConfig options { };
    options.image_count = VIDDEC_MAX_IMAGES;
    options.codec       = m_format == VIDEO_FORMAT_H264 ? ViddecCodec_H264 : ViddecCodec_Av1;
    options.width       = m_width;
    options.height      = m_height;
    // Service registration and stream cadence are independent. Use its declared
    // size/rate budget up front: 60 at 1080p, 30 at 4K. Submit every stream frame.
    const uint32_t budgetRate = uint32_t(
        std::min<uint64_t>(VIDEO_DECODER_MAX_FRAME_RATE, VIDEO_DECODER_PIXEL_RATE / (uint64_t(m_width) * m_height)));
    options.frame_rate        = std::min(uint32_t(m_fps), budgetRate);
    options.max_sample_size   = kMaxSample;
    options.color_policy      = 3; // The session explicitly requests limited-range Rec.709.
    options.codec_config      = config.empty() ? nullptr : config.data();
    options.codec_config_size = config.size();
    Result rc                 = videoDecoderCreate(&m_pool->decoder, &options);
    if (R_FAILED(rc))
    {
        // The SDK unwinds partially-created resources on failure.
        brls::Logger::error(
            "SWITCH2_HWDEC: create failed, codec={} size={}x{} streamFPS={} registrationFPS={} rc=0x{:x}",
            int(options.codec), m_width, m_height, m_fps, options.frame_rate, rc);
        m_pool->decoder  = { };
        m_retry_after_ms = LiGetMillis() + 1000;
        return false;
    }
    m_pool->open = true;
    m_open       = true;
    m_need_idr   = false;
    brls::Logger::info("SWITCH2_HWDEC: initialized codec={} streamFPS={} registrationFPS={} (output not verified yet)",
        int(options.codec), m_fps, options.frame_rate);
    return true;
}

void Switch2VideoDecoder::fail(const char* operation, Result rc)
{
    brls::Logger::error("SWITCH2_HWDEC: {} failed rc=0x{:x}; requesting a fresh keyframe", operation, rc);
    closeHardware();
    m_need_idr = true;
}

bool Switch2VideoDecoder::drain(uint64_t timeout)
{
    if (!m_open)
        return true;
    m_pool->releaseFrames();
    // Limit each batch, allowing the submitter to acquire the mutex at 120 FPS.
    for (unsigned count = 0; count < VIDEO_DECODER_DEFAULT_IMAGES; ++count)
    {
        const uint64_t before = LiGetMicroseconds();
        VideoFrame frame { };
        Result rc = videoDecoderGetFrame(&m_pool->decoder, &frame, count ? 0 : timeout);
        if (timedOut(rc))
            return true;
        if (R_FAILED(rc))
        {
            fail("acquire", rc);
            return false;
        }
        AVFrame* output = nullptr;
        bool copied     = false;
        if (frame.width > 0 && frame.height > 0 && !(frame.width & 1) && !(frame.height & 1)
            && frame.width <= unsigned(m_width) && frame.height <= unsigned(m_height))
        {
            output = AVFrameHolder::instance().acquireWriteFrame();
            if (output)
            {
                output->format      = AV_PIX_FMT_NV12;
                output->width       = frame.width;
                output->height      = frame.height;
                output->color_range = AVCOL_RANGE_MPEG;
                output->colorspace  = AVCOL_SPC_BT709;
                output->pts         = frame.timestamp_ns;
                if (m_format == VIDEO_FORMAT_H264 && frame.planes[0] && frame.planes[1])
                {
                    copied = switch2AttachNativeFrame(output, m_pool, frame);
                }
                else if (av_frame_get_buffer(output, 32) >= 0)
                {
                    // This is hardware decode with copy-back, not zero-copy GPU interop.
                    rc     = videoDecoderCopyFrame(&m_pool->decoder, &frame, output->data[0], output->linesize[0],
                        output->data[1], output->linesize[1]);
                    copied = R_SUCCEEDED(rc);
                }
            }
        }
        const int64_t frameTimestamp = frame.timestamp_ns;
        if (!output || output->format != kSwitch2NativeFrame)
            videoDecoderReleaseFrame(&m_pool->decoder, &frame);
        if (!copied)
        {
            if (output)
                AVFrameHolder::instance().recycleWriteFrame(output);
            fail("copy/allocate output", rc ? rc : MAKERESULT(Module_Libnx, LibnxError_BadInput));
            return false;
        }
        const int visibleWidth = output->width, visibleHeight = output->height;
        AVFrameHolder::instance().pushTransferred(output);
        switch2WakeVideoFrame();
        const uint64_t now      = LiGetMillis();
        const uint64_t copyTime = LiGetMicroseconds() - before;
        m_progress.current_decode_time += copyTime;
        m_progress.total_decode_time += copyTime;
        auto timing = std::find_if(m_pending_times.begin(), m_pending_times.end(),
            [frameTimestamp](const auto& item) { return item.first == frameTimestamp; });
        if (timing != m_pending_times.end())
        {
            const uint32_t delay = uint32_t(now - timing->second);
            m_progress.current_decoder_delay_time += delay;
            m_progress.total_decoder_delay_time += delay;
            m_pending_times.erase(m_pending_times.begin(), std::next(timing));
        }
        m_progress.current_decoded_frames++;
        m_progress.total_decoded_frames++;
        if (!m_first_frame)
        {
            brls::Logger::info("SWITCH2_HWDEC: first hardware frame ready, {}x{}, requestedFPS={}", visibleWidth,
                visibleHeight, m_fps);
            m_first_frame = true;
        }
    }
    return true;
}

void Switch2VideoDecoder::readFrames()
{
    while (!m_stopped.load())
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            drain(kPollNs);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

int Switch2VideoDecoder::submit_decode_unit(PDECODE_UNIT unit)
{
    if (m_software)
        return m_software->submit_decode_unit(unit);
    if (!unit || unit->fullLength <= 0 || size_t(unit->fullLength) > kMaxSample)
        return DR_NEED_IDR;
    std::vector<uint8_t> sample, sps, pps;
    sample.reserve(unit->fullLength);
    unsigned entries = 0;
    for (PLENTRY entry = unit->bufferList; entry; entry = entry->next)
    {
        if (++entries > 4096 || !entry->data || entry->length <= 0
            || size_t(entry->length) > size_t(unit->fullLength) - sample.size())
            return DR_NEED_IDR;
        auto begin = reinterpret_cast<const uint8_t*>(entry->data);
        sample.insert(sample.end(), begin, begin + entry->length);
        if (entry->bufferType == BUFFER_TYPE_SPS)
            sps.insert(sps.end(), begin, begin + entry->length);
        if (entry->bufferType == BUFFER_TYPE_PPS)
            pps.insert(pps.end(), begin, begin + entry->length);
    }
    if (sample.size() != size_t(unit->fullLength))
        return DR_NEED_IDR;
    std::lock_guard<std::mutex> lock(m_mutex);
    const uint32_t number = uint32_t(unit->frameNumber);
    if (m_have_last_frame && number > m_last_frame && number - m_last_frame > 1)
        m_progress.network_dropped_frames += number - m_last_frame - 1;
    m_last_frame      = number;
    m_have_last_frame = true;
    m_progress.current_received_frames++;
    m_progress.total_received_frames++;
    m_progress.total_frames = m_progress.total_received_frames + m_progress.network_dropped_frames;
    const uint64_t now      = LiGetMillis();
    const uint64_t received = unit->receiveTimeUs / 1000;
    const auto reassembly   = uint32_t(now >= received ? std::min<uint64_t>(now - received, 60000) : 0);
    m_progress.current_reassembly_time += reassembly;
    m_progress.total_reassembly_time += reassembly;
    if (m_need_idr)
    {
        if (now < m_retry_after_ms)
            return DR_NEED_IDR;
        if (unit->frameType != FRAME_TYPE_IDR)
            return DR_NEED_IDR;
        if (m_format == VIDEO_FORMAT_H264 && (sps.empty() || pps.empty()))
            return DR_NEED_IDR;
        // The SDK requires four-byte start codes in H.264 codec configuration.
        auto normalize = [](std::vector<uint8_t>& nal)
        {
            if (nal.size() >= 4 && nal[0] == 0 && nal[1] == 0 && nal[2] == 0 && nal[3] == 1)
                return true;
            if (nal.size() >= 3 && nal[0] == 0 && nal[1] == 0 && nal[2] == 1)
            {
                nal.insert(nal.begin(), 0);
                return true;
            }
            return false;
        };
        if (m_format == VIDEO_FORMAT_H264 && (!normalize(sps) || !normalize(pps)))
            return DR_NEED_IDR;
        sps.insert(sps.end(), pps.begin(), pps.end());
        if (!openHardware(sps))
            return DR_NEED_IDR;
    }
    if (!drain(0))
        return DR_NEED_IDR;
    m_pool->releaseFrames(); // Queue trimming during drain may have freed output slots.
    const auto stamp = int64_t(
        std::min<uint64_t>(unit->presentationTimeUs, uint64_t(std::numeric_limits<int64_t>::max()) / 1000) * 1000);
    const Result rc = videoDecoderSubmit(&m_pool->decoder, sample.data(), sample.size(), stamp, false, kSubmitNs);
    if (R_FAILED(rc))
    {
        // A timed-out submission may have accepted a prefix. Never retry the
        // complete sample, which would duplicate bytes; recreate on the next IDR.
        fail("submit", rc);
        return DR_NEED_IDR;
    }
    m_pending_times.emplace_back(stamp, now);
    if (m_pending_times.size() > 64)
    {
        fail("output stalled", KERNELRESULT(TimedOut));
        return DR_NEED_IDR;
    }
    drain(0);
    return m_need_idr ? DR_NEED_IDR : DR_OK;
}

int Switch2VideoDecoder::capabilities() const { return CAPABILITY_SLICES_PER_FRAME(4); }

int Switch2VideoDecoder::hardwareFormats() { return VIDEO_FORMAT_H264 | VIDEO_FORMAT_AV1_MAIN8; }

int Switch2VideoDecoder::softwareFormats()
{
    int formats = 0;
    if (avcodec_find_decoder(AV_CODEC_ID_H264))
        formats |= VIDEO_FORMAT_H264;
    if (avcodec_find_decoder(AV_CODEC_ID_HEVC))
        formats |= VIDEO_FORMAT_H265 | VIDEO_FORMAT_H265_MAIN10;
    if (avcodec_find_decoder(AV_CODEC_ID_AV1))
        formats |= VIDEO_FORMAT_AV1_MAIN8 | VIDEO_FORMAT_AV1_MAIN10;
    return formats;
}

VideoDecodeStats* Switch2VideoDecoder::video_decode_stats()
{
    if (m_software)
        return m_software->video_decode_stats();
    std::lock_guard<std::mutex> lock(m_mutex);
    const uint64_t now = LiGetMillis();
    if (now - m_window_ms >= 1000)
    {
        m_snapshot                      = m_progress;
        const float seconds             = float(now - m_window_ms) / 1000;
        m_snapshot.current_received_fps = m_progress.current_received_frames / seconds;
        m_snapshot.current_decoded_fps  = m_progress.current_decoded_frames / seconds;
        m_snapshot.current_host_fps
            = now > m_started_ms ? float(m_progress.total_frames) * 1000 / float(now - m_started_ms) : 0;
        m_snapshot.current_decoding_time = m_progress.current_decoded_frames
            ? float(m_progress.current_decode_time) / 1000.0f / m_progress.current_decoded_frames
            : 0;
        m_snapshot.current_decoder_delay = m_progress.current_decoded_frames
            ? float(m_progress.current_decoder_delay_time) / m_progress.current_decoded_frames
            : 0;
        m_snapshot.session_decoding_time = m_progress.total_decoded_frames
            ? float(m_progress.total_decode_time) / 1000.0f / m_progress.total_decoded_frames
            : 0;
        m_snapshot.session_decoder_delay = m_progress.total_decoded_frames
            ? float(m_progress.total_decoder_delay_time) / m_progress.total_decoded_frames
            : 0;
        m_snapshot.current_receive_time  = m_progress.current_received_frames
            ? float(m_progress.current_reassembly_time) / m_progress.current_received_frames
            : 0;
        m_snapshot.session_receive_time  = m_progress.total_received_frames
            ? float(m_progress.total_reassembly_time) / m_progress.total_received_frames
            : 0;
        brls::Logger::info("SWITCH2_HWDEC: requested={} received={:.1f} decoded={:.1f} total={}", m_fps,
            m_snapshot.current_received_fps, m_snapshot.current_decoded_fps, m_progress.total_decoded_frames);
        m_progress.current_received_frames = m_progress.current_decoded_frames = 0;
        m_progress.current_reassembly_time                                     = 0;
        m_progress.current_decode_time = m_progress.current_decoder_delay_time = 0;
        m_window_ms                                                            = now;
    }
    return &m_snapshot;
}
#endif
