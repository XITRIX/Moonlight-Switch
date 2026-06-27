static const char* metal_shader = R"glsl(
#include <metal_stdlib>
using namespace metal;

struct Vertex
{
    float4 position [[ position ]];
    float2 texCoords;
};

struct CscParams
{
    float3 matrix[3];
    float3 offsets;
    float bitnessScaleFactor;
};

struct PostProcessParams
{
    float4 control;
};

struct RcasParams
{
    float4 control;
};

struct EasuParams
{
    float4 con0;
    float4 con1;
    float4 con2;
    float4 con3;
};

constexpr sampler s(coord::normalized, address::clamp_to_edge, filter::linear);

vertex Vertex vs_draw(constant Vertex *vertices [[ buffer(0) ]], uint id [[ vertex_id ]])
{
    return vertices[id];
}

fragment float4 ps_draw_biplanar(Vertex v [[ stage_in ]],
                                 constant CscParams &cscParams [[ buffer(0) ]],
                                 texture2d<float> luminancePlane [[ texture(0) ]],
                                 texture2d<float> chrominancePlane [[ texture(1) ]])
{
    float3 yuv = float3(luminancePlane.sample(s, v.texCoords).r,
                        chrominancePlane.sample(s, v.texCoords).rg);
    yuv *= cscParams.bitnessScaleFactor;
    yuv -= cscParams.offsets;

    float3 rgb;
    rgb.r = dot(yuv, cscParams.matrix[0]);
    rgb.g = dot(yuv, cscParams.matrix[1]);
    rgb.b = dot(yuv, cscParams.matrix[2]);
    return float4(rgb, 1.0f);
}

fragment float4 ps_draw_triplanar(Vertex v [[ stage_in ]],
                                  constant CscParams &cscParams [[ buffer(0) ]],
                                  texture2d<float> luminancePlane [[ texture(0) ]],
                                  texture2d<float> chrominancePlaneU [[ texture(1) ]],
                                  texture2d<float> chrominancePlaneV [[ texture(2) ]])
{
    float3 yuv = float3(luminancePlane.sample(s, v.texCoords).r,
                        chrominancePlaneU.sample(s, v.texCoords).r,
                        chrominancePlaneV.sample(s, v.texCoords).r);
    yuv *= cscParams.bitnessScaleFactor;
    yuv -= cscParams.offsets;

    float3 rgb;
    rgb.r = dot(yuv, cscParams.matrix[0]);
    rgb.g = dot(yuv, cscParams.matrix[1]);
    rgb.b = dot(yuv, cscParams.matrix[2]);
    return float4(rgb, 1.0f);
}

fragment float4 ps_draw_rgb(Vertex v [[ stage_in ]],
                            texture2d<float> rgbTexture [[ texture(0) ]])
{
    return rgbTexture.sample(s, v.texCoords);
}

float interleavedGradientNoise(float2 uv)
{
    return fract(52.9829189f * fract(dot(uv, float2(0.06711056f, 0.00583715f))));
}

float3 applyDither(float3 rgb, float strength, float2 position)
{
    rgb += (strength / 255.0f) * interleavedGradientNoise(position) -
           (strength / 510.0f);
    return clamp(rgb, 0.0f, 1.0f);
}

fragment float4 ps_draw_postprocess(Vertex v [[ stage_in ]],
                                    constant PostProcessParams &params [[ buffer(0) ]],
                                    texture2d<float> inputTexture [[ texture(0) ]])
{
    float3 rgb = inputTexture.sample(s, v.texCoords).rgb;

    if (params.control.x > 0.5f) {
        rgb = applyDither(rgb, params.control.y, v.position.xy);
    }

    return float4(rgb, 1.0f);
}

constant float FSR_RCAS_LIMIT = 0.25f - (1.0f / 16.0f);

float APrxMedRcpF1(float a)
{
    return 1.0f / max(a, 1.0e-6f);
}

float ARcpF1(float x)
{
    float denom = abs(x) < 1.0e-6f ? (x < 0.0f ? -1.0e-6f : 1.0e-6f) : x;
    return 1.0f / denom;
}

float ASatF1(float x)
{
    return clamp(x, 0.0f, 1.0f);
}

float AMax3F1(float x, float y, float z)
{
    return max(x, max(y, z));
}

float AMin3F1(float x, float y, float z)
{
    return min(x, min(y, z));
}

float APrxLoRcpF1(float a)
{
    return 1.0f / max(a, 1.0e-6f);
}

float APrxLoRsqF1(float a)
{
    return rsqrt(max(a, 1.0e-12f));
}

float3 AMax3F3(float3 x, float3 y, float3 z)
{
    return max(x, max(y, z));
}

float3 AMin3F3(float3 x, float3 y, float3 z)
{
    return min(x, min(y, z));
}

int2 FsrEasuClampCoord(texture2d<float> inputTexture, int2 p)
{
    int2 size = int2(int(inputTexture.get_width()), int(inputTexture.get_height()));
    return clamp(p, int2(0), size - int2(1));
}

float3 FsrEasuLoadF(texture2d<float> inputTexture, int2 p)
{
    return inputTexture.read(uint2(FsrEasuClampCoord(inputTexture, p))).rgb;
}

void FsrEasuTapF(thread float3 &aC,
                 thread float &aW,
                 float2 off,
                 float2 dir,
                 float2 len,
                 float lob,
                 float clp,
                 float3 c)
{
    float2 v;
    v.x = (off.x * dir.x) + (off.y * dir.y);
    v.y = (off.x * (-dir.y)) + (off.y * dir.x);
    v *= len;
    float d2 = v.x * v.x + v.y * v.y;
    d2 = min(d2, clp);
    float wB = (2.0f / 5.0f) * d2 - 1.0f;
    float wA = lob * d2 - 1.0f;
    wB *= wB;
    wA *= wA;
    wB = (25.0f / 16.0f) * wB - (25.0f / 16.0f - 1.0f);
    float w = wB * wA;
    aC += c * w;
    aW += w;
}

void FsrEasuSetF(thread float2 &dir,
                 thread float &len,
                 float2 pp,
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
    float w = 0.0f;
    if (biS) {
        w = (1.0f - pp.x) * (1.0f - pp.y);
    }
    if (biT) {
        w = pp.x * (1.0f - pp.y);
    }
    if (biU) {
        w = (1.0f - pp.x) * pp.y;
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

float3 FsrEasuF(texture2d<float> inputTexture, uint2 ip, constant EasuParams &params)
{
    float2 pp = float2(ip) * params.con0.xy + params.con0.zw;
    float2 ppFloor = floor(pp);
    int2 f = int2(ppFloor);
    pp -= ppFloor;

    float3 b = FsrEasuLoadF(inputTexture, f + int2(0, -1));
    float3 c = FsrEasuLoadF(inputTexture, f + int2(1, -1));
    float3 e = FsrEasuLoadF(inputTexture, f + int2(-1, 0));
    float3 ff = FsrEasuLoadF(inputTexture, f + int2(0, 0));
    float3 g = FsrEasuLoadF(inputTexture, f + int2(1, 0));
    float3 h = FsrEasuLoadF(inputTexture, f + int2(2, 0));
    float3 i = FsrEasuLoadF(inputTexture, f + int2(-1, 1));
    float3 j = FsrEasuLoadF(inputTexture, f + int2(0, 1));
    float3 k = FsrEasuLoadF(inputTexture, f + int2(1, 1));
    float3 l = FsrEasuLoadF(inputTexture, f + int2(2, 1));
    float3 n = FsrEasuLoadF(inputTexture, f + int2(0, 2));
    float3 o = FsrEasuLoadF(inputTexture, f + int2(1, 2));

    float bL = b.b * 0.5f + (b.r * 0.5f + b.g);
    float cL = c.b * 0.5f + (c.r * 0.5f + c.g);
    float iL = i.b * 0.5f + (i.r * 0.5f + i.g);
    float jL = j.b * 0.5f + (j.r * 0.5f + j.g);
    float fL = ff.b * 0.5f + (ff.r * 0.5f + ff.g);
    float eL = e.b * 0.5f + (e.r * 0.5f + e.g);
    float kL = k.b * 0.5f + (k.r * 0.5f + k.g);
    float lL = l.b * 0.5f + (l.r * 0.5f + l.g);
    float hL = h.b * 0.5f + (h.r * 0.5f + h.g);
    float gL = g.b * 0.5f + (g.r * 0.5f + g.g);
    float oL = o.b * 0.5f + (o.r * 0.5f + o.g);
    float nL = n.b * 0.5f + (n.r * 0.5f + n.g);

    float2 dir = float2(0.0f);
    float len = 0.0f;
    FsrEasuSetF(dir, len, pp, true, false, false, false, bL, eL, fL, gL, jL);
    FsrEasuSetF(dir, len, pp, false, true, false, false, cL, fL, gL, hL, kL);
    FsrEasuSetF(dir, len, pp, false, false, true, false, fL, iL, jL, kL, nL);
    FsrEasuSetF(dir, len, pp, false, false, false, true, gL, jL, kL, lL, oL);

    float2 dir2 = dir * dir;
    float dirR = dir2.x + dir2.y;
    bool zro = dirR < (1.0f / 32768.0f);
    dirR = APrxLoRsqF1(dirR);
    dirR = zro ? 1.0f : dirR;
    dir.x = zro ? 1.0f : dir.x;
    dir *= float2(dirR);
    len = len * 0.5f;
    len *= len;
    float stretch = (dir.x * dir.x + dir.y * dir.y) *
                    APrxLoRcpF1(max(abs(dir.x), abs(dir.y)));
    float2 len2 = float2(1.0f + (stretch - 1.0f) * len,
                         1.0f - 0.5f * len);
    float lob = 0.5f + ((1.0f / 4.0f - 0.04f) - 0.5f) * len;
    float clp = APrxLoRcpF1(lob);

    float3 min4 = min(AMin3F3(ff, g, j), k);
    float3 max4 = max(AMax3F3(ff, g, j), k);
    float3 aC = float3(0.0f);
    float aW = 0.0f;
    FsrEasuTapF(aC, aW, float2(0.0f, -1.0f) - pp, dir, len2, lob, clp, b);
    FsrEasuTapF(aC, aW, float2(1.0f, -1.0f) - pp, dir, len2, lob, clp, c);
    FsrEasuTapF(aC, aW, float2(-1.0f, 1.0f) - pp, dir, len2, lob, clp, i);
    FsrEasuTapF(aC, aW, float2(0.0f, 1.0f) - pp, dir, len2, lob, clp, j);
    FsrEasuTapF(aC, aW, float2(0.0f, 0.0f) - pp, dir, len2, lob, clp, ff);
    FsrEasuTapF(aC, aW, float2(-1.0f, 0.0f) - pp, dir, len2, lob, clp, e);
    FsrEasuTapF(aC, aW, float2(1.0f, 1.0f) - pp, dir, len2, lob, clp, k);
    FsrEasuTapF(aC, aW, float2(2.0f, 1.0f) - pp, dir, len2, lob, clp, l);
    FsrEasuTapF(aC, aW, float2(2.0f, 0.0f) - pp, dir, len2, lob, clp, h);
    FsrEasuTapF(aC, aW, float2(1.0f, 0.0f) - pp, dir, len2, lob, clp, g);
    FsrEasuTapF(aC, aW, float2(1.0f, 2.0f) - pp, dir, len2, lob, clp, o);
    FsrEasuTapF(aC, aW, float2(0.0f, 2.0f) - pp, dir, len2, lob, clp, n);

    return min(max4, max(min4, aC * float3(ARcpF1(aW))));
}

fragment float4 ps_draw_easu(Vertex v [[ stage_in ]],
                             constant EasuParams &params [[ buffer(0) ]],
                             texture2d<float> inputTexture [[ texture(0) ]])
{
    return float4(FsrEasuF(inputTexture, uint2(v.position.xy), params), 1.0f);
}

float3 FsrRcasLoadF(texture2d<float> inputTexture, int2 p)
{
    int2 size = int2(inputTexture.get_width(), inputTexture.get_height());
    return inputTexture.read(uint2(clamp(p, int2(0), size - int2(1)))).rgb;
}

void FsrRcasInputF(thread float &r, thread float &g, thread float &b)
{
}

float3 FsrRcasF(texture2d<float> inputTexture, uint2 ip, float sharpnessLinear)
{
    int2 sp = int2(ip);
    float3 b = FsrRcasLoadF(inputTexture, sp + int2(0, -1));
    float3 d = FsrRcasLoadF(inputTexture, sp + int2(-1, 0));
    float3 e = FsrRcasLoadF(inputTexture, sp);
    float3 f = FsrRcasLoadF(inputTexture, sp + int2(1, 0));
    float3 h = FsrRcasLoadF(inputTexture, sp + int2(0, 1));

    float bR = b.r;
    float bG = b.g;
    float bB = b.b;
    float dR = d.r;
    float dG = d.g;
    float dB = d.b;
    float eR = e.r;
    float eG = e.g;
    float eB = e.b;
    float fR = f.r;
    float fG = f.g;
    float fB = f.b;
    float hR = h.r;
    float hG = h.g;
    float hB = h.b;

    FsrRcasInputF(bR, bG, bB);
    FsrRcasInputF(dR, dG, dB);
    FsrRcasInputF(eR, eG, eB);
    FsrRcasInputF(fR, fG, fB);
    FsrRcasInputF(hR, hG, hB);

    float bL = bB * 0.5f + (bR * 0.5f + bG);
    float dL = dB * 0.5f + (dR * 0.5f + dG);
    float eL = eB * 0.5f + (eR * 0.5f + eG);
    float fL = fB * 0.5f + (fR * 0.5f + fG);
    float hL = hB * 0.5f + (hR * 0.5f + hG);

    float nz = 0.25f * bL + 0.25f * dL + 0.25f * fL + 0.25f * hL - eL;
    nz = ASatF1(abs(nz) * APrxMedRcpF1(
        AMax3F1(AMax3F1(bL, dL, eL), fL, hL) -
        AMin3F1(AMin3F1(bL, dL, eL), fL, hL)));
    nz = -0.5f * nz + 1.0f;

    float mn4R = min(AMin3F1(bR, dR, fR), hR);
    float mn4G = min(AMin3F1(bG, dG, fG), hG);
    float mn4B = min(AMin3F1(bB, dB, fB), hB);
    float mx4R = max(AMax3F1(bR, dR, fR), hR);
    float mx4G = max(AMax3F1(bG, dG, fG), hG);
    float mx4B = max(AMax3F1(bB, dB, fB), hB);

    float2 peakC = float2(1.0f, -4.0f);
    float hitMinR = min(mn4R, eR) * ARcpF1(4.0f * mx4R);
    float hitMinG = min(mn4G, eG) * ARcpF1(4.0f * mx4G);
    float hitMinB = min(mn4B, eB) * ARcpF1(4.0f * mx4B);
    float hitMaxR = (peakC.x - max(mx4R, eR)) * ARcpF1(4.0f * mn4R + peakC.y);
    float hitMaxG = (peakC.x - max(mx4G, eG)) * ARcpF1(4.0f * mn4G + peakC.y);
    float hitMaxB = (peakC.x - max(mx4B, eB)) * ARcpF1(4.0f * mn4B + peakC.y);
    float lobeR = max(-hitMinR, hitMaxR);
    float lobeG = max(-hitMinG, hitMaxG);
    float lobeB = max(-hitMinB, hitMaxB);
    float lobe = max(-FSR_RCAS_LIMIT,
                     min(AMax3F1(lobeR, lobeG, lobeB), 0.0f)) *
                 sharpnessLinear;

    lobe *= nz;

    float rcpL = APrxMedRcpF1(4.0f * lobe + 1.0f);
    float3 pix;
    pix.r = (lobe * bR + lobe * dR + lobe * hR + lobe * fR + eR) * rcpL;
    pix.g = (lobe * bG + lobe * dG + lobe * hG + lobe * fG + eG) * rcpL;
    pix.b = (lobe * bB + lobe * dB + lobe * hB + lobe * fB + eB) * rcpL;
    return pix;
}

fragment float4 ps_draw_rcas(Vertex v [[ stage_in ]],
                             constant RcasParams &params [[ buffer(0) ]],
                             texture2d<float> inputTexture [[ texture(0) ]])
{
    float3 color = FsrRcasF(inputTexture, uint2(v.position.xy), params.control.x);
    return float4(color, 1.0f);
}

)glsl";
