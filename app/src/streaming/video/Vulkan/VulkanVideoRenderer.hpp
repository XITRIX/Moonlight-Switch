#pragma once
#ifdef __SWITCH2__
#include "IVideoRenderer.hpp"
#include "Switch2VulkanContext.hpp"
class VulkanVideoRenderer : public IVideoRenderer
{
  public:
    ~VulkanVideoRenderer() override;
    void draw(NVGcontext*, int width, int height, AVFrame* frame, int format) override;
    VideoRenderStats* video_render_stats() override { return &stats; }

  private:
    void initialize();
    VkDescriptorSetLayout descriptors = VK_NULL_HANDLE;
    VkDescriptorPool pool             = VK_NULL_HANDLE;
    VkPipelineLayout layout           = VK_NULL_HANDLE;
    VkPipeline pipeline               = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> sets { };
    std::array<Switch2VkBuffer, 2> uploads { };
    VideoRenderStats stats { }, progress { };
    uint64_t nativeDraws = 0, copiedDraws = 0;
};
#endif
