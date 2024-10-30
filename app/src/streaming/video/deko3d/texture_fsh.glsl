#version 460

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D plane0;
layout (binding = 1) uniform sampler2D plane1;

layout (std140, binding = 2) uniform Transformation
{
    mat3 yuvmat;
    vec3 offset;
    vec4 uv_data;
} u;

void main()
{
    vec2 uv = (vTextureCoord - u.uv_data.xy) * u.uv_data.zw;
    vec3 YCbCr = vec3(texture(plane0, uv).r, texture(plane1, uv).r, texture(plane2, uv).r) - u.offset;
    outColor = vec4(clamp(u.yuvmat * YCbCr, 0.0, 1.0), 1.0);
}
