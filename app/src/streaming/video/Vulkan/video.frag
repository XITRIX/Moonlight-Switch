#version 450
#ifdef TEXEL_BUFFER_SAMPLING
layout(set = 0, binding = 1) uniform textureBuffer lumaPlane;
layout(set = 0, binding = 2) uniform textureBuffer chromaPlane;
#else
layout(set = 0, binding = 0, std430) readonly buffer Pixels { uint words[]; } pixels;
#endif
layout(push_constant) uniform Params
{
    uvec4 plane;
    uvec4 extra;
    vec4 region;
    vec4 red;
    vec4 green;
    vec4 blue;
}
p;
layout(location = 0) in vec2 position;
layout(location = 0) out vec4 color;
float byteAt(uint address)
{
#ifdef TEXEL_BUFFER_SAMPLING
    return texelFetch(lumaPlane, int(address)).r;
#else
    return float((pixels.words[address >> 2] >> ((address & 3) * 8)) & 255) / 255.0;
#endif
}
vec3 sampleAt(ivec2 pos, bool chroma)
{
    ivec2 size = ivec2(p.extra.zw) / (chroma ? 2 : 1);
    pos        = clamp(pos, ivec2(0), size - 1);
    if (!chroma)
        return vec3(byteAt(p.plane.x + uint(pos.y) * p.plane.y + uint(pos.x)), 0, 0);
    uint address = p.plane.z + uint(pos.y) * p.plane.w;
    if (p.red.w < 0.5)
#ifdef TEXEL_BUFFER_SAMPLING
        return vec3(0, texelFetch(chromaPlane, int((address + uint(pos.x) * 2) >> 1)).rg);
#else
        return vec3(0, byteAt(address + uint(pos.x) * 2), byteAt(address + uint(pos.x) * 2 + 1));
#endif
    return vec3(0, byteAt(address + uint(pos.x)), byteAt(p.extra.x + uint(pos.y) * p.extra.y + uint(pos.x)));
}
vec3 filtered(vec2 uv, bool chroma)
{
    vec2 q     = uv * vec2(p.extra.zw) / (chroma ? 2.0 : 1.0) - 0.5;
    ivec2 base = ivec2(floor(q));
    vec2 f     = fract(q);
    return mix(mix(sampleAt(base, chroma), sampleAt(base + ivec2(1, 0), chroma), f.x),
        mix(sampleAt(base + ivec2(0, 1), chroma), sampleAt(base + ivec2(1, 1), chroma), f.x), f.y);
}
void main()
{
    vec2 uv = (position - p.region.xy) * p.region.zw;
    if (any(lessThan(uv, vec2(0))) || any(greaterThan(uv, vec2(1))))
    {
        color = vec4(0, 0, 0, 1);
        return;
    }
    vec3 yuv = filtered(uv, false) + filtered(uv, true) - vec3(p.green.w, 128.0 / 255.0, 128.0 / 255.0);
    color    = vec4(clamp(vec3(dot(p.red.xyz, yuv), dot(p.green.xyz, yuv), dot(p.blue.xyz, yuv)), 0, 1), 1);
}
