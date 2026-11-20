#ifdef __SWITCH2__
#include <switch.h>

#include <borealis.hpp>

#include "Switch2ImageInterop.hpp"
extern "C"
{
#include <switch2/services/gsv.h>
#include <switch2/services/viddec.h>

    Result __real_gsvGpuRegisterMemory(GsvGpu*, u64, u64, u8, u32*);
    Result __wrap_gsvGpuRegisterMemory(GsvGpu* gpu, u64 address, u64 size, u8 client, u32* handle)
    {
        Result rc = __real_gsvGpuRegisterMemory(gpu, address, size, client, handle);
        if (R_SUCCEEDED(rc) && handle)
            switch2RecordGpuRange(address, size, *handle);
        return rc;
    }
    Result __real_gsvCreateNativeMemory(GsvNativeMemory*, void*, u64);
    Result __wrap_gsvCreateNativeMemory(GsvNativeMemory* out, void* address, u64 size)
    {
        Result rc = 0;
        if (switch2CreateSharedNativeMemory(out, address, size, &rc))
            return rc;
        return __real_gsvCreateNativeMemory(out, address, size);
    }

    Result __real_viddecInitialize(void);
    Result __wrap_viddecInitialize(void)
    {
        Result rc = __real_viddecInitialize();
        brls::Logger::info("HWDEC_STAGE: creator rc=0x{:x}", rc);
        return rc;
    }
    Result __real_gsvRegisterFrame(u64 mode, u64* permitted);
    Result __wrap_gsvRegisterFrame(u64 mode, u64* permitted)
    {
        Result rc = __real_gsvRegisterFrame(mode, permitted);
        brls::Logger::info("HWDEC_STAGE: frame mode={} permitted={} rc=0x{:x}", mode,
            R_SUCCEEDED(rc) && permitted ? *permitted : 0, rc);
        return rc;
    }
    Result __real_viddecImagePoolCreate(ViddecImagePool*, const GsvDecodeImagePreference*, u32, void*, u64);
    Result __wrap_viddecImagePoolCreate(
        ViddecImagePool* out, const GsvDecodeImagePreference* preference, u32 count, void* backing, u64 size)
    {
        const bool imported = switch2PreimportDecoderImages && switch2PreimportImage(backing, size);
        Result rc           = __real_viddecImagePoolCreate(out, preference, count, backing, size);
        if (R_FAILED(rc) && imported)
        {
            brls::Logger::warning("SWITCH2_VK_IMPORT: native pool registration rejected preimported memory rc=0x{:x}; "
                                  "removing GPU import",
                rc);
            switch2UnimportImage(backing);
            // Retry only a fully unwound pool; never duplicate a live registration.
            if (!out->memory.memory_id && !out->base_image_id)
                rc = __real_viddecImagePoolCreate(out, preference, count, backing, size);
        }
        brls::Logger::info("HWDEC_STAGE: image pool count={} bytes={} rc=0x{:x}", count, size, rc);
        return rc;
    }
    Result __real_viddecImagePoolClose(ViddecImagePool*);
    Result __wrap_viddecImagePoolClose(ViddecImagePool* pool)
    {
        // VideoDecoderClose stops the native session before destroying its pools.
        // Frame leases ensure the renderer has also finished before this happens.
        void* backing = pool->backing;
        Result rc     = __real_viddecImagePoolClose(pool);
        if (R_SUCCEEDED(rc))
            switch2UnimportImage(backing);
        return rc;
    }
    Result __real_viddecOpenDecoder(ViddecDecoder*);
    Result __wrap_viddecOpenDecoder(ViddecDecoder* out)
    {
        Result rc = __real_viddecOpenDecoder(out);
        brls::Logger::info("HWDEC_STAGE: decoder session rc=0x{:x}", rc);
        return rc;
    }
    Result __real_viddecDecoderInitialize(ViddecDecoder*, const ViddecDecoderConfig*, void*, u64, const void*, size_t);
    Result __wrap_viddecDecoderInitialize(ViddecDecoder* d, const ViddecDecoderConfig* config, void* work, u64 workSize,
        const void* codecConfig, size_t codecSize)
    {
        brls::Logger::info("HWDEC_STAGE: configure codec={} {}x{} fps={} color={} maxSample={} work={} configBytes={}",
            int(config->codec), config->width, config->height, config->frame_rate, config->input_format,
            config->max_input_size, workSize, codecSize);
        Result rc = __real_viddecDecoderInitialize(d, config, work, workSize, codecConfig, codecSize);
        brls::Logger::info("HWDEC_STAGE: configure rc=0x{:x}", rc);
        return rc;
    }
    Result __real_viddecDecoderRegisterImage(ViddecDecoder*, u32, u32);
    Result __wrap_viddecDecoderRegisterImage(ViddecDecoder* d, u32 slot, u32 image)
    {
        Result rc = __real_viddecDecoderRegisterImage(d, slot, image);
        brls::Logger::info("HWDEC_STAGE: register image slot={} rc=0x{:x}", slot, rc);
        return rc;
    }
    Result __real_viddecDecoderStart(ViddecDecoder* d);
    Result __wrap_viddecDecoderStart(ViddecDecoder* d)
    {
        Result rc = __real_viddecDecoderStart(d);
        brls::Logger::info("HWDEC_STAGE: start rc=0x{:x}", rc);
        return rc;
    }
}
#endif
