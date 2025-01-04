#ifdef USE_GL_RENDERER

#include "IVideoRenderer.hpp"
#if defined(__LIBRETRO__)
#include "glsym.h"
#elif defined(__PSV__)
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
extern "C"
{
#include <gpu_es4/psp2_pvr_hint.h>
#include <psp2/kernel/modulemgr.h>
}
#else
#include <glad/glad.h>
#endif
#pragma once

#define PLANES_NUM_MAX 3

class GLVideoRenderer : public IVideoRenderer {
  public:
    GLVideoRenderer(){};
    ~GLVideoRenderer();

    void draw(NVGcontext* vg, int width, int height, AVFrame* frame, int imageFormat) override;

    VideoRenderStats* video_render_stats() override;

  private:
    void bindTexture(int id);
    void initialize(AVFrame* frame);
    void checkAndInitialize(int width, int height, AVFrame* frame);
    void checkAndUpdateScale(int width, int height, AVFrame* frame);

    bool m_is_initialized = false;
    GLuint m_texture_id[PLANES_NUM_MAX] = {0, 0, 0};
    GLint m_texture_uniform[PLANES_NUM_MAX];
    GLuint m_shader_program;
    GLuint m_vbo, m_vao;
    int m_frame_width = 0;
    int m_frame_height = 0;
    int m_screen_width = 0;
    int m_screen_height = 0;
    int m_yuvmat_location;
    int m_offset_location;
    int m_uv_data_location;
    int textureWidth[PLANES_NUM_MAX];
    int textureHeight[PLANES_NUM_MAX];
    float borderColor[PLANES_NUM_MAX] = {0.0f, 0.5f, 0.5f};
    VideoRenderStats m_video_render_stats_progress = {};
    VideoRenderStats m_video_render_stats_cache = {};
    uint64_t timeCount = 0;

    int currentFrameTypePlanesNum = 0;
    const int (*currentPlanes)[5];
    int currentFormat;
};

#endif // USE_GL_RENDERER
