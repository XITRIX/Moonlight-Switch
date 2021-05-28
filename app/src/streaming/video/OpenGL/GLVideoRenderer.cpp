#include "GLVideoRenderer.hpp"
#include "borealis.hpp"

// TODO: GLES support

static const char *vertex_shader_string_core = "\
#version 140\n\
in vec2 position;\n\
out mediump vec2 tex_position;\n\
\
void main() {\n\
    gl_Position = vec4(position, 1, 1);\n\
    tex_position = vec2((position.x + 1.0) / 2.0, (1.0 - position.y) / 2.0);\n\
}";

static const char *fragment_shader_string_core = "\
#version 140\n\
uniform lowp sampler2D ymap;\n\
uniform lowp sampler2D umap;\n\
uniform lowp sampler2D vmap;\n\
uniform mat3 yuvmat;\n\
uniform vec3 offset;\n\
in mediump vec2 tex_position;\n\
out vec4 FragColor;\n\
\
void main() {\n\
    vec3 YCbCr = vec3(\n\
        texture(ymap, tex_position).r,\n\
        texture(umap, tex_position).r - 0.0,\n\
        texture(vmap, tex_position).r - 0.0\n\
    );\n\
    YCbCr -= offset;\n\
    FragColor = vec4(clamp(yuvmat * YCbCr, 0.0, 1.0), 1.0);\n\
}";

static const char *vertex_shader_string = "\
#version 120\n\
attribute vec2 position;\n\
varying vec2 tex_position;\n\
\
void main() {\n\
    gl_Position = vec4(position, 1, 1);\n\
    tex_position = vec2((position.x + 1.0) / 2.0, (1.0 - position.y) / 2.0);\n\
}";

static const char *fragment_shader_string = "\
#version 120\n\
uniform sampler2D ymap;\n\
uniform sampler2D umap;\n\
uniform sampler2D vmap;\n\
uniform mat3 yuvmat;\n\
uniform vec3 offset;\n\
varying vec2 tex_position;\n\
\
void main() {\n\
    vec3 YCbCr = vec3(\n\
        texture(ymap, tex_position).r,\n\
        texture(umap, tex_position).r - 0.0,\n\
        texture(vmap, tex_position).r - 0.0\n\
    );\n\
    YCbCr -= offset;\n\
    gl_FragColor = vec4(clamp(yuvmat * YCbCr, 0.0, 1.0), 1.0);\n\
}";

static const float vertices[] = {
    -1.f, -1.f,
    1.f, -1.f,
    -1.f, 1.f,
    1.f, 1.f
};

static const char* texture_mappings[] = { "ymap", "umap", "vmap" };

static const float* gl_color_offset(bool color_full) {
    static const float limitedOffsets[] = { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f };
    static const float fullOffsets[] = { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f };
    return color_full ? fullOffsets : limitedOffsets;
}

static const float* gl_color_matrix(enum AVColorSpace color_space, bool color_full) {
    static const float bt601Lim[] = {
        1.1644f, 1.1644f, 1.1644f,
        0.0f, -0.3917f, 2.0172f,
        1.5960f, -0.8129f, 0.0f
    };
    static const float bt601Full[] = {
        1.0f, 1.0f, 1.0f,
        0.0f, -0.3441f, 1.7720f,
        1.4020f, -0.7141f, 0.0f
    };
    static const float bt709Lim[] = {
        1.1644f, 1.1644f, 1.1644f,
        0.0f, -0.2132f, 2.1124f,
        1.7927f, -0.5329f, 0.0f
    };
    static const float bt709Full[] = {
        1.0f, 1.0f, 1.0f,
        0.0f, -0.1873f, 1.8556f,
        1.5748f, -0.4681f, 0.0f
    };
    static const float bt2020Lim[] = {
        1.1644f, 1.1644f, 1.1644f,
        0.0f, -0.1874f, 2.1418f,
        1.6781f, -0.6505f, 0.0f
    };
    static const float bt2020Full[] = {
        1.0f, 1.0f, 1.0f,
        0.0f, -0.1646f, 1.8814f,
        1.4746f, -0.5714f, 0.0f
    };
    
    switch (color_space) {
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_BT470BG:
            return color_full ? bt601Full : bt601Lim;
        case AVCOL_SPC_BT709:
            return color_full ? bt709Full : bt709Lim;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL:
            return color_full ? bt2020Full : bt2020Lim;
        default:
            return bt601Lim;
    };
}

static void check_shader(GLuint handle) {
    GLint success = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &success);
    
    brls::Logger::info("GL: GL_COMPILE_STATUS: {}", success);
    
    if (!success) {
        GLint length = 0;
        glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &length);
        
        char* buffer = (char*)malloc(length);
        
        glGetShaderInfoLog(handle, length, &length, buffer);
        brls::Logger::error("GL: Compile shader error: {}", buffer);
        free(buffer);
    }
}

static bool use_core_shaders() {
    char* version = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    return version[0] == '3' || version[0] == '4';
}

GLVideoRenderer::~GLVideoRenderer() {
    brls::Logger::info("GL: Cleanup...");
    
    if (m_shader_program) {
        glDeleteProgram(m_shader_program);
    }
    
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
    }
    
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
    }
    
    for (int i = 0; i < 3; i++) {
        if (m_texture_id[i]) {
            glDeleteTextures(1, &m_texture_id[i]);
        }
    }
    
    brls::Logger::info("GL: Cleanup done!");
}

void GLVideoRenderer::initialize() {
    m_shader_program = glCreateProgram();
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    
    bool use_gl_core = use_core_shaders();
    
    glShaderSource(vert, 1, use_gl_core ? &vertex_shader_string_core : &vertex_shader_string, 0);
    glCompileShader(vert);
    check_shader(vert);
    
    glShaderSource(frag, 1, use_gl_core ? &fragment_shader_string_core : &fragment_shader_string, 0);
    glCompileShader(frag);
    check_shader(frag);
    
    glAttachShader(m_shader_program, vert);
    glAttachShader(m_shader_program, frag);
    glLinkProgram(m_shader_program);
    glDeleteShader(vert);
    glDeleteShader(frag);
    
    for (int i = 0; i < 3; i++) {
        m_texture_uniform[i] = glGetUniformLocation(m_shader_program, texture_mappings[i]);
    }
    
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    
    glUseProgram(m_shader_program);
    
    int position_location = glGetAttribLocation(m_shader_program, "position");
    glEnableVertexAttribArray(position_location);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    
    m_yuvmat_location = glGetUniformLocation(m_shader_program, "yuvmat");
    m_offset_location = glGetUniformLocation(m_shader_program, "offset");
}

void GLVideoRenderer::draw(int width, int height, AVFrame *frame) {
    if (!m_video_render_stats.rendered_frames) {
        m_video_render_stats.measurement_start_timestamp = LiGetMillis();
    }
    
    uint64_t before_render = LiGetMillis();
    
    if (!m_is_initialized) {
        brls::Logger::info("GL: GL: {}, GLSL: {}", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
        brls::Logger::info("GL: Init with width: {}, height: {}", width, height);
        
        initialize();
        m_is_initialized = true;
        
        brls::Logger::info("GL: Init done");
    }
    
    if (m_width != frame->width || m_height != frame->height) {
        m_width = frame->width;
        m_height = frame->height;
        
        for (int i = 0; i < 3; i++) {
            if (m_texture_id[i]) {
                glDeleteTextures(1, &m_texture_id[i]);
            }
        }
        
        glGenTextures(3, m_texture_id);
        
        for (int i = 0; i < 3; i++) {
            glBindTexture(GL_TEXTURE_2D, m_texture_id[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, i > 0 ? m_width / 2 : m_width, i > 0 ? m_height / 2 : m_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        }
    }
    
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(m_shader_program);
    
    glUniform3fv(m_offset_location, 1, gl_color_offset(frame->color_range == AVCOL_RANGE_JPEG));
    glUniformMatrix3fv(m_yuvmat_location, 1, GL_FALSE, gl_color_matrix(frame->colorspace, frame->color_range == AVCOL_RANGE_JPEG));
    
    for (int i = 0; i < 3; i++) {
        auto image = frame->data[i];
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_texture_id[i]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, i > 0 ? m_width / 2 : m_width, i > 0 ? m_height / 2 : m_height, GL_RED, GL_UNSIGNED_BYTE, image);
        glUniform1i(m_texture_uniform[i], i);
        glActiveTexture(GL_TEXTURE0);
    }
    
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    m_video_render_stats.total_render_time += LiGetMillis() - before_render;
    m_video_render_stats.rendered_frames++;
}

VideoRenderStats* GLVideoRenderer::video_render_stats() {
    m_video_render_stats.rendered_fps = (float)m_video_render_stats.rendered_frames / ((float)(LiGetMillis() - m_video_render_stats.measurement_start_timestamp) / 1000);
    return (VideoRenderStats*)&m_video_render_stats;
}
