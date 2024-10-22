static const char* vertex_shader_string_core = R"glsl(
#version 140
in vec2 position;
out mediump vec2 tex_position;
void main() {
    gl_Position = vec4(position, 1.0, 1.0);
    tex_position = vec2((position.x * 0.5 + 0.5), (0.5 - position.y * 0.5));
}
)glsl";

static const char* fragment_two_planes_shader_string_core = R"glsl(
#version 140
uniform lowp sampler2D plane0;
uniform lowp sampler2D plane1;
uniform mat3 yuvmat;
uniform vec3 offset;
uniform vec4 uv_data; 
in mediump vec2 tex_position;
out vec4 FragColor;

void main() {
    vec2 uv = (tex_position - uv_data.xy) * uv_data.zw;
    vec3 YCbCr = vec3(texture(plane0, uv).r, texture(plane1, uv).r, texture(plane1, uv).g) - offset;
    FragColor = vec4(clamp(yuvmat * YCbCr, 0.0, 1.0), 1.0);
}
)glsl";

static const char* fragment_three_planes_shader_string_core = R"glsl(
#version 140
uniform lowp sampler2D plane0;
uniform lowp sampler2D plane1;
uniform lowp sampler2D plane2;
uniform mat3 yuvmat;
uniform vec3 offset;
uniform vec4 uv_data; 
in mediump vec2 tex_position;
out vec4 FragColor;

void main() {
    vec2 uv = (tex_position - uv_data.xy) * uv_data.zw;
    vec3 YCbCr = vec3(texture(plane0, uv).r, texture(plane1, uv).r, texture(plane2, uv).r) - offset;
    FragColor = vec4(clamp(yuvmat * YCbCr, 0.0, 1.0), 1.0);
}
)glsl";

static const char* vertex_shader_string = R"glsl(#version 300 es
in vec2 position;
out vec2 tex_position;

void main() {
    gl_Position = vec4(position, 1, 1);
    tex_position = vec2((position.x * 0.5 + 0.5), (0.5 - position.y * 0.5));
}
)glsl";

static const char* fragment_two_planes_shader_string = R"glsl(#version 300 es
uniform sampler2D plane0;
uniform sampler2D plane1;
uniform highp mat3 yuvmat;
uniform highp vec3 offset;
uniform highp vec4 uv_data;
in highp vec2 tex_position;
out mediump vec4 fragmentColor;

void main() {
    highp vec2 uv = (tex_position - uv_data.xy) * uv_data.zw;
    highp vec3 YCbCr = vec3(texture(plane0, uv).r, texture(plane1, uv).r, texture(plane1, uv).g) - offset;
    fragmentColor = vec4(clamp(yuvmat * YCbCr, 0.0, 1.0), 1.0);
}
)glsl";

static const char* fragment_three_planes_shader_string = R"glsl(#version 300 es
uniform sampler2D plane0;
uniform sampler2D plane1;
uniform sampler2D plane2;
uniform highp mat3 yuvmat;
uniform highp vec3 offset;
uniform highp vec4 uv_data;
in highp vec2 tex_position;
out mediump vec4 fragmentColor;

void main() {
    highp vec2 uv = (tex_position - uv_data.xy) * uv_data.zw;
    highp vec3 YCbCr = vec3(texture(plane0, uv).r, texture(plane1, uv).r, texture(plane2, uv).r) - offset;
    fragmentColor = vec4(clamp(yuvmat * YCbCr, 0.0, 1.0), 1.0);
}
)glsl";
