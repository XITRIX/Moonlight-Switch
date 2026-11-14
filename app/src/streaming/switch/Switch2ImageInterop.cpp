#ifdef __SWITCH2__
#include "Switch2ImageInterop.hpp"

#include <switch.h>

#include <borealis.hpp>
#include <map>
#include <mutex>

#include "Switch2VulkanContext.hpp"
namespace
{
struct Range
{
    uint64_t address, size;
    uint32_t handle;
};
struct Image
{
    Switch2VkBuffer buffer;
    std::vector<Range> ranges;
};
std::mutex mutex;
std::map<void*, Image> images;
thread_local std::vector<Range>* registering = nullptr;
}
void switch2RecordGpuRange(uint64_t address, uint64_t size, uint32_t handle)
{
    if (registering)
        registering->push_back({ address, size, handle });
}
bool switch2PreimportImage(void* address, uint64_t size)
{
    auto* context = Switch2VulkanContext::current();
    if (!context)
        return false;
    std::lock_guard<std::mutex> lock(mutex);
    Image image;
    struct Capture
    {
        explicit Capture(std::vector<Range>& ranges) { registering = &ranges; }
        ~Capture() { registering = nullptr; }
    } capture(image.ranges);
    if (!context->importBuffer(address, size, image.buffer))
        return false;
    images.emplace(address, std::move(image));
    return true;
}
Switch2ImportedImage switch2FindImportedImage(void* address)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto found = images.find(address);
    if (found == images.end())
        return { };
    return { found->second.buffer.buffer, found->second.buffer.size };
}
void switch2UnimportImage(void* address)
{
    auto* context = Switch2VulkanContext::current();
    if (!context)
        return;
    std::lock_guard<std::mutex> lock(mutex);
    auto found = images.find(address);
    if (found == images.end())
        return;
    context->destroyBuffer(found->second.buffer);
    images.erase(found);
}
bool switch2CreateSharedNativeMemory(GsvNativeMemory* out, void* address, uint64_t size, Result* result)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto image = images.find(address);
    if (image == images.end())
        return false;
    const uint64_t cpu = reinterpret_cast<uintptr_t>(address);
    for (const auto& range : image->second.ranges)
    {
        if (cpu < range.address || size > range.size || cpu - range.address > range.size - size)
            continue;
        *out       = { };
        u32 memory = 0;
        Result rc  = gsvReserveImageMemory(3, size, &memory);
        if (R_SUCCEEDED(rc))
            rc = gsvRegisterImageMemory(memory, cpu, size, range.handle, cpu - range.address);
        if (R_FAILED(rc))
        {
            if (memory)
                gsvReleaseImageMemory(memory);
        }
        else
        {
            out->memory_id = memory;
            out->address   = address;
            out->size      = size;
            // Vulkan owns the attribute/pin. Native image memory borrows its
            // GPU registration and is destroyed before that registration.
            out->has_attribute = false;
        }
        *result = rc;
        brls::Logger::info("SWITCH2_VK_IMPORT: GPU-backed native image bytes={} rc=0x{:x}", size, rc);
        return true;
    }
    brls::Logger::warning("SWITCH2_VK_IMPORT: no single GPU range covers the decoder allocation");
    *result = MAKERESULT(Module_Libnx, LibnxError_BadInput);
    return true;
}
#endif
