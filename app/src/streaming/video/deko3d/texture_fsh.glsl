#version 460

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D plane0;
layout (binding = 1) uniform sampler2D plane1;

layout (std140, binding = 0) uniform Transformation
{
    mat3 yuvmat;
    vec3 offset;
    vec4 uv_data;
} u;

void main()
{
    vec2 uv = (vTextureCoord - u.uv_data.xy) * u.uv_data.zw;

    float y = texture(plane0, uv).r - (16.0 / 255.0);
    float u_chroma = texture(plane1, uv).r - (128.0 / 255.0);
    float v_chroma = texture(plane1, uv).g - (128.0 / 255.0);

    // Explicit BT.709 limited-range YUV -> RGB conversion.
    vec3 rgb;
    rgb.r = 1.1644 * y + 1.7927 * v_chroma;
    rgb.g = 1.1644 * y - 0.2132 * u_chroma - 0.5329 * v_chroma;
    rgb.b = 1.1644 * y + 2.1124 * u_chroma;

    outColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}