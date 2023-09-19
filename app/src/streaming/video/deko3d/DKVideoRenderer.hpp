#if defined(__SWITCH__) && defined(USE_DEKO3D)

#pragma once
#include "IVideoRenderer.hpp"
#include <deko3d.hpp>

#include <borealis.hpp>

class DKVideoRenderer : public IVideoRenderer {
  public:
    DKVideoRenderer(){
      brls::Logger::info("{}", __PRETTY_FUNCTION__);
    };
    ~DKVideoRenderer();

    void draw(NVGcontext* vg, int width, int height, AVFrame* frame) override;

    VideoRenderStats* video_render_stats() override;

  private:
    // void bindTexture(int id);
    void initialize();
    void checkAndInitialize(int width, int height, AVFrame* frame);
    // void checkAndUpdateScale(int width, int height, AVFrame* frame);

    bool m_is_initialized = false;

    dk::Device dev;
    dk::Queue queue;

    dk::UniqueMemBlock memblk;;
    dk::UniqueCmdBuf cmdbuf;
    int cmdbuf_slice = 0;

    dk::UniqueMemBlock shader_memblk;
    dk::Shader vert_sh, frag_sh;

    int m_frame_width = 0;
    int m_frame_height = 0;
    int m_screen_width = 0;
    int m_screen_height = 0;
    int textureWidth[3];
    int textureHeight[3];
    VideoRenderStats m_video_render_stats = {};
};

#endif // __SWITCH__