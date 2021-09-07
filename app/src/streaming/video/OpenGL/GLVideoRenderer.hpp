#include "IVideoRenderer.hpp"
#if defined(__LIBRETRO__)
#include "glsym.h"
#else
#include <glad/glad.h>
#endif
#pragma once

class GLVideoRenderer: public IVideoRenderer {
public:
    GLVideoRenderer() {};
    ~GLVideoRenderer();

    void draw(NVGcontext* vg, int width, int height, AVFrame *frame) override;

    VideoRenderStats* video_render_stats() override;

private:
    void bindTexture(int id);
    void initialize();
    void checkAndInitialize(int width, int height, AVFrame *frame);
    void checkAndUpdateScale(int width, int height, AVFrame *frame);

    bool m_is_initialized = false;
    GLuint m_texture_id[3] = {0, 0, 0};
    GLint m_texture_uniform[3];
    GLuint m_shader_program;
    GLuint m_vbo, m_vao;
    int m_frame_width = 0;
    int m_frame_height = 0;
    int m_screen_width = 0;
    int m_screen_height = 0;
    int m_yuvmat_location;
    int m_offset_location;
    int m_uv_data_location;
    int textureWidth[3];
    int textureHeight[3];
    float borderColor[3] = {0.0f, 0.5f, 0.5f};
    VideoRenderStats m_video_render_stats = {};
};
