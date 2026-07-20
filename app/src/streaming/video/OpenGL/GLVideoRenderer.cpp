#ifdef USE_GL_RENDERER

#include "GLVideoRenderer.hpp"
#include "borealis.hpp"

#include "GLShaders.hpp"

#include <cstring>

// tex width | frame width | frame height | from color space | to color space
#if defined(USE_GLES2)
static const int nv12Planes[][5] = {
    {1, 1, 1, GL_LUMINANCE, GL_LUMINANCE},              // Y
    {2, 2, 2, GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA},  // UV
    {0, 0, 0, 0, 0},                                    // NOT EXISTS
};

static const int yuv420Planes[][5] = {
    {1, 1, 1, GL_LUMINANCE, GL_LUMINANCE},  // Y
    {1, 2, 2, GL_LUMINANCE, GL_LUMINANCE},  // U
    {1, 2, 2, GL_LUMINANCE, GL_LUMINANCE},  // V
};
#else
static const int nv12Planes[][5] = {
    {1, 1, 1, GL_R8, GL_RED},  // Y
    {2, 2, 2, GL_RG8, GL_RG},  // UV
    {0, 0, 0, 0, 0},           // NOT EXISTS
};

static const int yuv420Planes[][5] = {
    {1, 1, 1, GL_R8, GL_RED},  // Y
    {1, 2, 2, GL_R8, GL_RED},  // U
    {1, 2, 2, GL_R8, GL_RED},  // V
};
#endif

#if !defined(USE_GLES2)
static const int p010Planes[][5] = {
    {1, 1, 2, GL_R16, GL_RED},  // Y
    {2, 2, 4, GL_RG16, GL_RG},  // UV
    {0, 0, 0, 0, 0},            // NOT EXISTS
};
#endif

static const float vertices[] = {-1.0f, -1.0f, 1.0f, -1.0f,
                                 -1.0f, 1.0f,  1.0f, 1.0f};

static const char* texture_mappings[] = {"plane0", "plane1", "plane2"};

static const float* gl_color_offset(bool color_full) {
    static const float limitedOffsets[] = {16.0f / 255.0f, 128.0f / 255.0f,
                                           128.0f / 255.0f};
    static const float fullOffsets[] = {0.0f, 128.0f / 255.0f, 128.0f / 255.0f};
    return color_full ? fullOffsets : limitedOffsets;
}

static const float* gl_color_matrix(enum AVColorSpace color_space,
                                    bool color_full) {
    static const float bt601Lim[] = {1.1644f, 1.1644f, 1.1644f,  0.0f, -0.3917f,
                                     2.0172f, 1.5960f, -0.8129f, 0.0f};
    static const float bt601Full[] = {
        1.0f, 1.0f, 1.0f, 0.0f, -0.3441f, 1.7720f, 1.4020f, -0.7141f, 0.0f};
    static const float bt709Lim[] = {1.1644f, 1.1644f, 1.1644f,  0.0f, -0.2132f,
                                     2.1124f, 1.7927f, -0.5329f, 0.0f};
    static const float bt709Full[] = {
        1.0f, 1.0f, 1.0f, 0.0f, -0.1873f, 1.8556f, 1.5748f, -0.4681f, 0.0f};
    static const float bt2020Lim[] = {1.1644f, 1.1644f,  1.1644f,
                                      0.0f,    -0.1874f, 2.1418f,
                                      1.6781f, -0.6505f, 0.0f};
    static const float bt2020Full[] = {
        1.0f, 1.0f, 1.0f, 0.0f, -0.1646f, 1.8814f, 1.4746f, -0.5714f, 0.0f};

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
    }
}

static void check_shader(GLuint handle) {
    GLint success = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &success);

#ifndef _WIN32
    brls::Logger::info("GL: GL_COMPILE_STATUS: {}", success);
#endif

    if (!success) {
        GLint length = 0;
        glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &length);

        char* buffer = (char*)malloc(length);

        glGetShaderInfoLog(handle, length, &length, buffer);

#ifndef _WIN32
        brls::Logger::error("GL: Compile shader error: {}", buffer);
#endif

        free(buffer);
    }
}

static bool use_core_shaders() {
    char* version = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    return version[0] == '3' || version[0] == '4';
}

GLVideoRenderer::~GLVideoRenderer() {

#ifndef _WIN32
    brls::Logger::info("GL: Cleanup...");
#endif

    if (m_shader_program) {
        glDeleteProgram(m_shader_program);
    }

    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
    }

#if !defined(USE_GLES2)
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
    }
#endif

    for (int i = 0; i < currentFrameTypePlanesNum; i++) {
        if (m_texture_id[i]) {
            glDeleteTextures(1, &m_texture_id[i]);
        }
    }

#ifndef _WIN32
    brls::Logger::info("GL: Cleanup done!");
#endif
}

void GLVideoRenderer::initialize(AVFrame* frame) {
    m_shader_program = glCreateProgram();
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

#if defined(USE_GLES2)
    glShaderSource(vert, 1, &vertex_shader_string_gles2, nullptr);
#else
    bool use_gl_core = use_core_shaders();
    glShaderSource(vert, 1,
        use_gl_core ? &vertex_shader_string_core : &vertex_shader_string,
        nullptr);
#endif
    glCompileShader(vert);
    check_shader(vert);

    switch (frame->format) {
        case AV_PIX_FMT_YUV420P:
            currentFrameTypePlanesNum = 3;
            currentPlanes = yuv420Planes;
            currentFormat = GL_UNSIGNED_BYTE;

#if defined(USE_GLES2)
            glShaderSource(frag, 1, &fragment_three_planes_shader_string_gles2, nullptr);
#else
            glShaderSource(frag, 1,
                use_gl_core ? &fragment_three_planes_shader_string_core
                            : &fragment_three_planes_shader_string, nullptr);
#endif
            break;
        case AV_PIX_FMT_NV12:
            currentFrameTypePlanesNum = 2;
            currentPlanes = nv12Planes;
            currentFormat = GL_UNSIGNED_BYTE;

#if defined(USE_GLES2)
            glShaderSource(frag, 1, &fragment_two_planes_shader_string_gles2, nullptr);
#else
            glShaderSource(frag, 1,
                use_gl_core ? &fragment_two_planes_shader_string_core
                            : &fragment_two_planes_shader_string, nullptr);
#endif
            break;
        case AV_PIX_FMT_P010:
#if defined(USE_GLES2)
            brls::Logger::error("GL: 10-bit P010 video is not supported by GLES 2");
            m_is_initialized = false;
            return;
#else
            currentFrameTypePlanesNum = 2;
            currentPlanes = p010Planes;
            currentFormat = GL_UNSIGNED_SHORT;

            glShaderSource(frag, 1,
                   use_gl_core ? &fragment_two_planes_shader_string_core
                               : &fragment_two_planes_shader_string, nullptr);
            break;
#endif
        default:
            brls::Logger::info("GL: Unknown frame format! - {}", frame->format);
            m_is_initialized = false;
            return;
    }

    glCompileShader(frag);
    check_shader(frag);

    glAttachShader(m_shader_program, vert);
    glAttachShader(m_shader_program, frag);

    glLinkProgram(m_shader_program);

    glDeleteShader(vert);
    glDeleteShader(frag);

    glGenBuffers(1, &m_vbo);
#if !defined(USE_GLES2)
    glGenVertexArrays(1, &m_vao);
#endif

    for (int i = 0; i < currentFrameTypePlanesNum; i++) {
        m_texture_uniform[i] =
            glGetUniformLocation(m_shader_program, texture_mappings[i]);
    }

    m_yuvmat_location = glGetUniformLocation(m_shader_program, "yuvmat");
    m_offset_location = glGetUniformLocation(m_shader_program, "offset");
    m_uv_data_location = glGetUniformLocation(m_shader_program, "uv_data");
}

void GLVideoRenderer::bindTexture(int id) {
    glBindTexture(GL_TEXTURE_2D, m_texture_id[id]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if !defined(USE_GLES2)
    float borderColorInternal[] = {borderColor[id], 0.0f, 0.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColorInternal);
#endif
    textureWidth[id] = m_frame_width / currentPlanes[id][1];
    textureHeight[id] = m_frame_height / currentPlanes[id][2];
    glTexImage2D(GL_TEXTURE_2D, 0, currentPlanes[id][3], textureWidth[id], textureHeight[id],
                 0, currentPlanes[id][4], currentFormat, nullptr);
    glUniform1i(m_texture_uniform[id], id);
}

void GLVideoRenderer::checkAndInitialize(int width, int height,
                                         AVFrame* frame) {
    if (!m_is_initialized) {
#ifndef _WIN32
//        brls::Logger::info("GL: GL: {}, GLSL: {}", glGetString(GL_VERSION),
//                           glGetString(GL_SHADING_LANGUAGE_VERSION));
        brls::Logger::info("GL: Init with width: {}, height: {}", width,
                           height);
#endif

        m_is_initialized = true;
        initialize(frame);

#ifndef _WIN32
        brls::Logger::info("GL: Init done");
#endif
    }
}

void GLVideoRenderer::checkAndUpdateScale(int width, int height,
                                          AVFrame* frame) {
    bool textureLayoutChanged =
        (m_frame_width != frame->width) ||
        (m_frame_height != frame->height) ||
        (m_screen_width != width) ||
        (m_screen_height != height);

#if !defined(__PSV__)
    // Some GLES implementations historically needed this refresh. Recreating
    // all YUV textures every frame is prohibitively expensive on Vita, so keep
    // them stable until the frame or viewport geometry actually changes.
    textureLayoutChanged = textureLayoutChanged || !use_core_shaders();
#endif

    if (textureLayoutChanged) {
        m_frame_width = frame->width;
        m_frame_height = frame->height;

        m_screen_width = width;
        m_screen_height = height;
    }

    // Borealis/NanoVG changes the active vertex buffer and attribute state
    // while drawing the in-game overlay. Restore our quad state on every
    // video draw, even when the texture dimensions have not changed.
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    if (textureLayoutChanged) {
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                     GL_STATIC_DRAW);
    }

    int positionLocation = glGetAttribLocation(m_shader_program, "position");
    glEnableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    if (textureLayoutChanged) {
        for (int i = 0; i < currentFrameTypePlanesNum; i++) {
            if (m_texture_id[i]) {
                glDeleteTextures(1, &m_texture_id[i]);
            }
        }

        glGenTextures(currentFrameTypePlanesNum, m_texture_id);

        for (int i = 0; i < currentFrameTypePlanesNum; i++) {
            bindTexture(i);
        }

        bool colorFull = frame->color_range == AVCOL_RANGE_JPEG;

        glUniform3fv(m_offset_location, 1, gl_color_offset(colorFull));
        glUniformMatrix3fv(m_yuvmat_location, 1, GL_FALSE,
                           gl_color_matrix(frame->colorspace, colorFull));

        float frameAspect = ((float)m_frame_height / (float)m_frame_width);
        float screenAspect = ((float)m_screen_height / (float)m_screen_width);

        if (frameAspect > screenAspect) {
            float multiplier = frameAspect / screenAspect;
            glUniform4f(m_uv_data_location, 0.5f - 0.5f * (1.0f / multiplier),
                        0.0f, multiplier, 1.0f);
        } else {
            float multiplier = screenAspect / frameAspect;
            glUniform4f(m_uv_data_location, 0.0f,
                        0.5f - 0.5f * (1.0f / multiplier), 1.0f, multiplier);
        }
    }
}

void GLVideoRenderer::draw(NVGcontext* vg, int width, int height,
                           AVFrame* frame, int imageFormat) {
    if (!m_video_render_stats_progress.rendered_frames) {
        m_video_render_stats_progress.measurement_start_timestamp = LiGetMillis();
    }

    uint64_t before_render = LiGetMillis();

    checkAndInitialize(width, height, frame);
    if (!m_is_initialized) {
        return;
    }

#if !defined(USE_GLES2)
    glBindVertexArray(m_vao);
#endif

    glUseProgram(m_shader_program);
    checkAndUpdateScale(width, height, frame);

    glClearColor(1, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    for (int i = 0; i < currentFrameTypePlanesNum; i++) {
        const uint8_t* image = frame->data[i];
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_texture_id[i]);
#if defined(USE_GLES2)
        const int bytesPerComponent = currentFormat == GL_UNSIGNED_SHORT ? 2 : 1;
        const size_t rowBytes = static_cast<size_t>(textureWidth[i]) *
                                currentPlanes[i][0] * bytesPerComponent;
        if (static_cast<size_t>(frame->linesize[i]) != rowBytes) {
            uploadBuffer[i].resize(rowBytes * textureHeight[i]);
            for (int row = 0; row < textureHeight[i]; row++) {
                std::memcpy(uploadBuffer[i].data() + rowBytes * row,
                    image + frame->linesize[i] * row, rowBytes);
            }
            image = uploadBuffer[i].data();
        }
#else
        int real_width = frame->linesize[i] / currentPlanes[i][0];
        glPixelStorei(GL_UNPACK_ROW_LENGTH, real_width);
#endif
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth[i],
                        textureHeight[i], currentPlanes[i][4], currentFormat, image);
        glActiveTexture(GL_TEXTURE0);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    auto render_time = LiGetMillis() - before_render;

    m_video_render_stats_progress.total_render_time += render_time;
    m_video_render_stats_progress.rendered_frames++;

    const int time_interval = 200;
    const uint64_t now = LiGetMillis();
    if (now - m_video_render_stats_progress.measurement_start_timestamp >=
        time_interval) {
        // brls::Logger::debug("FPS: {}", frames / 5.0f);
        m_video_render_stats_cache = m_video_render_stats_progress;
        m_video_render_stats_progress = {};

        const uint64_t elapsed_time =
            now - m_video_render_stats_cache.measurement_start_timestamp;
        m_video_render_stats_cache.rendered_fps =
                elapsed_time && m_video_render_stats_cache.rendered_frames > 1
                ? (float)(m_video_render_stats_cache.rendered_frames - 1) /
                      ((float)elapsed_time / 1000)
                : 0.0f;

        m_video_render_stats_cache.rendering_time = (float)m_video_render_stats_cache.total_render_time /
                (float) m_video_render_stats_cache.rendered_frames;
    }

//    auto code = glGetError();
//    brls::Logger::error("OpenGL error: {}\n", code);
}

VideoRenderStats* GLVideoRenderer::video_render_stats() {
    return (VideoRenderStats*)&m_video_render_stats_cache;
}

#endif // USE_GL_RENDERER
