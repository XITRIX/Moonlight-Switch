#ifdef __SWITCH2__
#include "VulkanVideoRenderer.hpp"

#include <borealis.hpp>
#include <cstring>
#include <stdexcept>

#include "Switch2FrameReferences.hpp"
#include "Switch2ImageInterop.hpp"
#include "Switch2NativeFrame.hpp"
#include "video_spirv.hpp"

namespace
{
struct Params
{
    uint32_t plane[4], extra[4];
    float region[4], red[4], green[4], blue[4];
};
static_assert(sizeof(Params) == 96);
VkShaderModule shader(Switch2VulkanContext& ctx, const uint32_t* code, size_t size)
{
    VkShaderModuleCreateInfo info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info.codeSize = size;
    info.pCode    = code;
    VkShaderModule result;
    ctx.check(vkCreateShaderModule(ctx.device, &info, nullptr, &result), "video shader");
    return result;
}
}

void VulkanVideoRenderer::initialize()
{
    auto& ctx = *Switch2VulkanContext::current();
    VkDescriptorSetLayoutBinding binding { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr };
    VkDescriptorSetLayoutCreateInfo descriptorInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    descriptorInfo.bindingCount = 1;
    descriptorInfo.pBindings    = &binding;
    ctx.check(vkCreateDescriptorSetLayout(ctx.device, &descriptorInfo, nullptr, &descriptors), "video descriptors");
    VkPushConstantRange constants { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Params) };
    VkPipelineLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &descriptors;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &constants;
    ctx.check(vkCreatePipelineLayout(ctx.device, &layoutInfo, nullptr, &layout), "video layout");
    VkDescriptorPoolSize size { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 };
    VkDescriptorPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.maxSets       = 2;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &size;
    ctx.check(vkCreateDescriptorPool(ctx.device, &poolInfo, nullptr, &pool), "video pool");
    VkDescriptorSetLayout layouts[] { descriptors, descriptors };
    VkDescriptorSetAllocateInfo allocation { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocation.descriptorPool     = pool;
    allocation.descriptorSetCount = sets.size();
    allocation.pSetLayouts        = layouts;
    ctx.check(vkAllocateDescriptorSets(ctx.device, &allocation, sets.data()), "video sets");
    const VkShaderModule vert = shader(ctx, video_vert, sizeof(video_vert)),
                         frag = shader(ctx, video_frag, sizeof(video_frag));
    VkPipelineShaderStageCreateInfo stages[2] { };
    for (auto& stage : stages)
    {
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.pName = "main";
    }
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    VkPipelineVertexInputStateCreateInfo vertex { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo assembly { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth   = 1;
    VkPipelineMultisampleStateCreateInfo msaa { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    VkPipelineColorBlendAttachmentState color { };
    color.colorWriteMask = 15;
    VkPipelineColorBlendStateCreateInfo blend { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blend.attachmentCount = 1;
    blend.pAttachments    = &color;
    VkDynamicState dynamicStates[] { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates    = dynamicStates;
    VkGraphicsPipelineCreateInfo info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    info.stageCount          = 2;
    info.pStages             = stages;
    info.pVertexInputState   = &vertex;
    info.pInputAssemblyState = &assembly;
    info.pViewportState      = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState   = &msaa;
    info.pDepthStencilState  = &depth;
    info.pColorBlendState    = &blend;
    info.pDynamicState       = &dynamic;
    info.layout              = layout;
    info.renderPass          = ctx.renderPass;
    auto rc                  = vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    vkDestroyShaderModule(ctx.device, vert, nullptr);
    vkDestroyShaderModule(ctx.device, frag, nullptr);
    ctx.check(rc, "video pipeline");
}

VulkanVideoRenderer::~VulkanVideoRenderer()
{
    auto* ctx = Switch2VulkanContext::current();
    if (!ctx)
        return;
    std::lock_guard<std::mutex> lock(ctx->queueMutex);
    vkDeviceWaitIdle(ctx->device);
    for (auto& upload : uploads)
        ctx->destroyBuffer(upload);
    if (pipeline)
        vkDestroyPipeline(ctx->device, pipeline, nullptr);
    if (layout)
        vkDestroyPipelineLayout(ctx->device, layout, nullptr);
    if (pool)
        vkDestroyDescriptorPool(ctx->device, pool, nullptr);
    if (descriptors)
        vkDestroyDescriptorSetLayout(ctx->device, descriptors, nullptr);
}

void VulkanVideoRenderer::draw(NVGcontext*, int, int, AVFrame* frame, int)
{
    auto* context = Switch2VulkanContext::current();
    if (!context || !frame)
        return;
    auto& ctx = *context;
    if (!pipeline)
        initialize();
    const uint64_t before = LiGetMillis();
    Params params { };
    params.extra[2]            = frame->width;
    params.extra[3]            = frame->height;
    Switch2NativeFrame* native = switch2NativeFrame(frame);
    const uint8_t* planes[3] { };
    int pitch[3] { };
    int format = frame->format;
    Switch2ImportedImage imported { };
    if (native)
    {
        format            = AV_PIX_FMT_NV12;
        const auto& image = native->pool->decoder.images[native->frame.slot];
        imported          = switch2FindImportedImage(image.backing);
        const auto& desc  = image.descriptor;
        if (imported.buffer)
        {
            for (unsigned i = 0; i < 2; ++i)
            {
                const uint64_t rows = unsigned(frame->height) >> i;
                if (!rows || desc.planes[i].pitch < unsigned(frame->width)
                    || uint64_t(desc.planes[i].offset) + desc.planes[i].span > imported.size
                    || (rows - 1) * desc.planes[i].pitch + frame->width > desc.planes[i].span)
                    throw std::runtime_error("Invalid native NV12 import geometry");
            }
            params.plane[0] = desc.planes[0].offset;
            params.plane[1] = desc.planes[0].pitch;
            params.plane[2] = desc.planes[1].offset;
            params.plane[3] = desc.planes[1].pitch;
        }
        for (int i = 0; i < 2; ++i)
        {
            planes[i] = native->frame.planes[i];
            pitch[i]  = native->frame.pitches[i];
        }
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            planes[i] = frame->data[i];
            pitch[i]  = frame->linesize[i];
        }
    }
    if (format != AV_PIX_FMT_NV12 && format != AV_PIX_FMT_YUV420P)
        throw std::runtime_error("Unsupported Vulkan video pixel format");
    VkBuffer buffer    = imported.buffer;
    VkDeviceSize bytes = imported.size;
    if (!buffer)
    {
        const uint32_t yBytes  = frame->width * frame->height;
        const uint32_t uvBytes = yBytes / 2;
        bytes                  = (VkDeviceSize(yBytes) + uvBytes + 3) & ~VkDeviceSize(3);
        auto& upload           = uploads[ctx.frameIndex];
        if (upload.size < bytes)
        {
            ctx.destroyBuffer(upload);
            upload = ctx.createBuffer(bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        }
        const int count = format == AV_PIX_FMT_NV12 ? 2 : 3;
        uint32_t offset = 0;
        for (int i = 0; i < count; ++i)
        {
            const int width  = i && format == AV_PIX_FMT_YUV420P ? frame->width / 2 : frame->width;
            const int height = i ? frame->height / 2 : frame->height;
            if (!planes[i] || pitch[i] < width)
                throw std::runtime_error("Invalid software video plane");
            for (int row = 0; row < height; ++row)
                std::memcpy(static_cast<uint8_t*>(upload.mapped) + offset + size_t(row) * width,
                    planes[i] + ptrdiff_t(row) * pitch[i], width);
            if (i < 2)
            {
                params.plane[i * 2]     = offset;
                params.plane[i * 2 + 1] = width;
            }
            else
            {
                params.extra[0] = offset;
                params.extra[1] = width;
            }
            offset += width * height;
        }
        buffer = upload.buffer;
        ++copiedDraws;
    }
    else
    {
        ctx.retain(switch2FrameReference(frame->opaque_ref));
        ++nativeDraws;
    }
    const float aspect = float(ctx.extent.width) * frame->height / (float(ctx.extent.height) * frame->width);
    params.region[2] = params.region[3] = 1;
    if (aspect > 1)
    {
        params.region[0] = 0.5f - 0.5f / aspect;
        params.region[2] = aspect;
    }
    else
    {
        params.region[1] = 0.5f - 0.5f * aspect;
        params.region[3] = 1 / aspect;
    }
    const bool full   = frame->color_range == AVCOL_RANGE_JPEG;
    const bool rec709 = frame->colorspace == AVCOL_SPC_BT709;
    params.red[0] = params.green[0] = params.blue[0] = full ? 1 : 1.1644f;
    params.red[2]                                    = rec709 ? (full ? 1.5748f : 1.7927f) : (full ? 1.402f : 1.596f);
    params.green[1] = rec709 ? (full ? -0.1873f : -0.2132f) : (full ? -0.3441f : -0.3917f);
    params.green[2] = rec709 ? (full ? -0.4681f : -0.5329f) : (full ? -0.7141f : -0.8129f);
    params.blue[1]  = rec709 ? (full ? 1.8556f : 2.1124f) : (full ? 1.772f : 2.0172f);
    params.red[3]   = format == AV_PIX_FMT_YUV420P ? 1 : 0;
    params.green[3] = full ? 0 : 16.0f / 255;
    VkDescriptorBufferInfo descriptor { buffer, 0, bytes };
    VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet          = sets[ctx.frameIndex];
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo     = &descriptor;
    vkUpdateDescriptorSets(ctx.device, 1, &write, 0, nullptr);
    VkViewport viewport { 0, 0, float(ctx.extent.width), float(ctx.extent.height), 0, 1 };
    VkRect2D scissor { { 0, 0 }, ctx.extent };
    vkCmdSetViewport(ctx.command(), 0, 1, &viewport);
    vkCmdSetScissor(ctx.command(), 0, 1, &scissor);
    vkCmdBindPipeline(ctx.command(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(
        ctx.command(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &sets[ctx.frameIndex], 0, nullptr);
    vkCmdPushConstants(ctx.command(), layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(params), &params);
    vkCmdDraw(ctx.command(), 3, 1, 0, 0);
    const auto now = LiGetMillis();
    if (!progress.rendered_frames)
        progress.measurement_start_timestamp = before;
    ++progress.rendered_frames;
    progress.total_render_time += now - before;
    const auto elapsed = now - progress.measurement_start_timestamp;
    if (elapsed >= 1000)
    {
        stats                = progress;
        stats.rendered_fps   = float(progress.rendered_frames) * 1000 / elapsed;
        stats.rendering_time = float(progress.total_render_time) / progress.rendered_frames;
        progress             = { };
        brls::Logger::info("SWITCH2_VULKAN_VIDEO: directBufferDraws={} copyDraws={} renderFPS={}", nativeDraws,
            copiedDraws, stats.rendered_fps);
    }
}
#endif
