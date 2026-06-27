#version 460

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D inputTexture;

layout (std140, binding = 0) uniform EasuConstants
{
    uvec4 con0;
    uvec4 con1;
    uvec4 con2;
    uvec4 con3;
} u;

float APrxLoRcpF1(float a)
{
    return uintBitsToFloat(0x7ef07ebbu - floatBitsToUint(a));
}

float APrxLoRsqF1(float a)
{
    return uintBitsToFloat(0x5f347d74u - (floatBitsToUint(a) >> 1u));
}

float ARcpF1(float x)
{
    return 1.0 / x;
}

float ASatF1(float x)
{
    return clamp(x, 0.0, 1.0);
}

vec3 AMax3F3(vec3 x, vec3 y, vec3 z)
{
    return max(x, max(y, z));
}

vec3 AMin3F3(vec3 x, vec3 y, vec3 z)
{
    return min(x, min(y, z));
}

ivec2 FsrEasuClampCoord(ivec2 p)
{
    ivec2 size = textureSize(inputTexture, 0);
    return clamp(p, ivec2(0), size - ivec2(1));
}

vec3 FsrEasuLoadF(ivec2 p)
{
    return texelFetch(inputTexture, FsrEasuClampCoord(p), 0).rgb;
}

void FsrEasuTapF(
    inout vec3 aC,
    inout float aW,
    vec2 off,
    vec2 dir,
    vec2 len,
    float lob,
    float clp,
    vec3 c)
{
    vec2 v;
    v.x = (off.x * dir.x) + (off.y * dir.y);
    v.y = (off.x * (-dir.y)) + (off.y * dir.x);
    v *= len;
    float d2 = v.x * v.x + v.y * v.y;
    d2 = min(d2, clp);
    float wB = (2.0 / 5.0) * d2 - 1.0;
    float wA = lob * d2 - 1.0;
    wB *= wB;
    wA *= wA;
    wB = (25.0 / 16.0) * wB - (25.0 / 16.0 - 1.0);
    float w = wB * wA;
    aC += c * w;
    aW += w;
}

void FsrEasuSetF(
    inout vec2 dir,
    inout float len,
    vec2 pp,
    bool biS,
    bool biT,
    bool biU,
    bool biV,
    float lA,
    float lB,
    float lC,
    float lD,
    float lE)
{
    float w = 0.0;
    if (biS) {
        w = (1.0 - pp.x) * (1.0 - pp.y);
    }
    if (biT) {
        w = pp.x * (1.0 - pp.y);
    }
    if (biU) {
        w = (1.0 - pp.x) * pp.y;
    }
    if (biV) {
        w = pp.x * pp.y;
    }

    float dc = lD - lC;
    float cb = lC - lB;
    float lenX = max(abs(dc), abs(cb));
    lenX = APrxLoRcpF1(lenX);
    float dirX = lD - lB;
    dir.x += dirX * w;
    lenX = ASatF1(abs(dirX) * lenX);
    lenX *= lenX;
    len += lenX * w;

    float ec = lE - lC;
    float ca = lC - lA;
    float lenY = max(abs(ec), abs(ca));
    lenY = APrxLoRcpF1(lenY);
    float dirY = lE - lA;
    dir.y += dirY * w;
    lenY = ASatF1(abs(dirY) * lenY);
    lenY *= lenY;
    len += lenY * w;
}

void FsrEasuF(out vec3 pix, uvec2 ip, uvec4 con0, uvec4 con1, uvec4 con2, uvec4 con3)
{
    vec2 pp = vec2(ip) * uintBitsToFloat(con0.xy) + uintBitsToFloat(con0.zw);
    ivec2 f = ivec2(floor(pp));
    pp -= floor(pp);

    vec3 b = FsrEasuLoadF(f + ivec2(0, -1));
    vec3 c = FsrEasuLoadF(f + ivec2(1, -1));
    vec3 e = FsrEasuLoadF(f + ivec2(-1, 0));
    vec3 ff = FsrEasuLoadF(f + ivec2(0, 0));
    vec3 g = FsrEasuLoadF(f + ivec2(1, 0));
    vec3 h = FsrEasuLoadF(f + ivec2(2, 0));
    vec3 i = FsrEasuLoadF(f + ivec2(-1, 1));
    vec3 j = FsrEasuLoadF(f + ivec2(0, 1));
    vec3 k = FsrEasuLoadF(f + ivec2(1, 1));
    vec3 l = FsrEasuLoadF(f + ivec2(2, 1));
    vec3 n = FsrEasuLoadF(f + ivec2(0, 2));
    vec3 o = FsrEasuLoadF(f + ivec2(1, 2));

    float bL = b.b * 0.5 + (b.r * 0.5 + b.g);
    float cL = c.b * 0.5 + (c.r * 0.5 + c.g);
    float iL = i.b * 0.5 + (i.r * 0.5 + i.g);
    float jL = j.b * 0.5 + (j.r * 0.5 + j.g);
    float fL = ff.b * 0.5 + (ff.r * 0.5 + ff.g);
    float eL = e.b * 0.5 + (e.r * 0.5 + e.g);
    float kL = k.b * 0.5 + (k.r * 0.5 + k.g);
    float lL = l.b * 0.5 + (l.r * 0.5 + l.g);
    float hL = h.b * 0.5 + (h.r * 0.5 + h.g);
    float gL = g.b * 0.5 + (g.r * 0.5 + g.g);
    float oL = o.b * 0.5 + (o.r * 0.5 + o.g);
    float nL = n.b * 0.5 + (n.r * 0.5 + n.g);

    vec2 dir = vec2(0.0);
    float len = 0.0;
    FsrEasuSetF(dir, len, pp, true, false, false, false, bL, eL, fL, gL, jL);
    FsrEasuSetF(dir, len, pp, false, true, false, false, cL, fL, gL, hL, kL);
    FsrEasuSetF(dir, len, pp, false, false, true, false, fL, iL, jL, kL, nL);
    FsrEasuSetF(dir, len, pp, false, false, false, true, gL, jL, kL, lL, oL);

    vec2 dir2 = dir * dir;
    float dirR = dir2.x + dir2.y;
    bool zro = dirR < (1.0 / 32768.0);
    dirR = APrxLoRsqF1(dirR);
    dirR = zro ? 1.0 : dirR;
    dir.x = zro ? 1.0 : dir.x;
    dir *= vec2(dirR);
    len = len * 0.5;
    len *= len;
    float stretch = (dir.x * dir.x + dir.y * dir.y) *
                    APrxLoRcpF1(max(abs(dir.x), abs(dir.y)));
    vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
    float clp = APrxLoRcpF1(lob);

    vec3 min4 = min(AMin3F3(ff, g, j), k);
    vec3 max4 = max(AMax3F3(ff, g, j), k);
    vec3 aC = vec3(0.0);
    float aW = 0.0;
    FsrEasuTapF(aC, aW, vec2(0.0, -1.0) - pp, dir, len2, lob, clp, b);
    FsrEasuTapF(aC, aW, vec2(1.0, -1.0) - pp, dir, len2, lob, clp, c);
    FsrEasuTapF(aC, aW, vec2(-1.0, 1.0) - pp, dir, len2, lob, clp, i);
    FsrEasuTapF(aC, aW, vec2(0.0, 1.0) - pp, dir, len2, lob, clp, j);
    FsrEasuTapF(aC, aW, vec2(0.0, 0.0) - pp, dir, len2, lob, clp, ff);
    FsrEasuTapF(aC, aW, vec2(-1.0, 0.0) - pp, dir, len2, lob, clp, e);
    FsrEasuTapF(aC, aW, vec2(1.0, 1.0) - pp, dir, len2, lob, clp, k);
    FsrEasuTapF(aC, aW, vec2(2.0, 1.0) - pp, dir, len2, lob, clp, l);
    FsrEasuTapF(aC, aW, vec2(2.0, 0.0) - pp, dir, len2, lob, clp, h);
    FsrEasuTapF(aC, aW, vec2(1.0, 0.0) - pp, dir, len2, lob, clp, g);
    FsrEasuTapF(aC, aW, vec2(1.0, 2.0) - pp, dir, len2, lob, clp, o);
    FsrEasuTapF(aC, aW, vec2(0.0, 2.0) - pp, dir, len2, lob, clp, n);

    pix = min(max4, max(min4, aC * vec3(ARcpF1(aW))));
}

void main()
{
    vec3 rgb;
    FsrEasuF(rgb, uvec2(gl_FragCoord.xy), u.con0, u.con1, u.con2, u.con3);
    outColor = vec4(rgb, 1.0);
}