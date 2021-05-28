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
    
    void draw(int width, int height, AVFrame *frame) override;
    
    VideoRenderStats* video_render_stats() override;
    
private:
    void initialize();
    
    bool m_is_initialized = false;
    GLuint m_texture_id[3] = {0, 0, 0}, m_texture_uniform[3];
    GLuint m_shader_program;
    GLuint m_vbo, m_vao;
    int m_width = 0, m_height = 0;
    int m_yuvmat_location, m_offset_location;
    VideoRenderStats m_video_render_stats = {};
};
