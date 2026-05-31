#version 460

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D inputTexture;

layout (std140, binding = 0) uniform RcasConstants
{
    vec4 control;
} u;

vec3 fetchClamped(ivec2 coord, ivec2 size)
{
    return texelFetch(inputTexture, clamp(coord, ivec2(0), size - ivec2(1)), 0).rgb;
}

void main()
{
    ivec2 size = textureSize(inputTexture, 0);
    ivec2 centerCoord = clamp(ivec2(gl_FragCoord.xy), ivec2(0), size - ivec2(1));

    vec3 center = fetchClamped(centerCoord, size);
    vec3 north = fetchClamped(centerCoord + ivec2(0, -1), size);
    vec3 west = fetchClamped(centerCoord + ivec2(-1, 0), size);
    vec3 east = fetchClamped(centerCoord + ivec2(1, 0), size);
    vec3 south = fetchClamped(centerCoord + ivec2(0, 1), size);

    vec3 minimum = min(center, min(min(north, west), min(east, south)));
    vec3 maximum = max(center, max(max(north, west), max(east, south)));
    vec3 blur = 0.25 * (north + west + east + south);
    vec3 edge = center - blur;

    vec3 headroom = min(center, 1.0 - center);
    vec3 contrast = max(maximum - minimum, vec3(1e-4));
    vec3 limiter = clamp(headroom / contrast, 0.0, 1.0);
    vec3 sharpened = center + edge * (u.control.x * 2.0) * limiter;

    outColor = vec4(clamp(sharpened, 0.0, 1.0), 1.0);
}