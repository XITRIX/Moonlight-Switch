#version 460

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D inputTexture;

layout (std140, binding = 0) uniform RcasConstants
{
    uvec4 control;
} u;

const float FSR_RCAS_LIMIT = 0.25 - (1.0 / 16.0);

float APrxMedRcpF1(float a)
{
    float b = uintBitsToFloat(0x7ef19fffu - floatBitsToUint(a));
    return b * (-b * a + 2.0);
}

float ARcpF1(float x)
{
    return 1.0 / x;
}

float ASatF1(float x)
{
    return clamp(x, 0.0, 1.0);
}

float AMax3F1(float x, float y, float z)
{
    return max(x, max(y, z));
}

float AMin3F1(float x, float y, float z)
{
    return min(x, min(y, z));
}

vec4 FsrRcasLoadF(ivec2 p)
{
    ivec2 size = textureSize(inputTexture, 0);
    return texelFetch(inputTexture, clamp(p, ivec2(0), size - ivec2(1)), 0);
}

void FsrRcasInputF(inout float r, inout float g, inout float b)
{
}

void FsrRcasF(out float pixR, out float pixG, out float pixB, uvec2 ip, uvec4 con)
{
    ivec2 sp = ivec2(ip);
    vec3 b = FsrRcasLoadF(sp + ivec2(0, -1)).rgb;
    vec3 d = FsrRcasLoadF(sp + ivec2(-1, 0)).rgb;
    vec3 e = FsrRcasLoadF(sp).rgb;
    vec3 f = FsrRcasLoadF(sp + ivec2(1, 0)).rgb;
    vec3 h = FsrRcasLoadF(sp + ivec2(0, 1)).rgb;

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

    float bL = bB * 0.5 + (bR * 0.5 + bG);
    float dL = dB * 0.5 + (dR * 0.5 + dG);
    float eL = eB * 0.5 + (eR * 0.5 + eG);
    float fL = fB * 0.5 + (fR * 0.5 + fG);
    float hL = hB * 0.5 + (hR * 0.5 + hG);

    float nz = 0.25 * bL + 0.25 * dL + 0.25 * fL + 0.25 * hL - eL;
    nz = ASatF1(abs(nz) * APrxMedRcpF1(
        AMax3F1(AMax3F1(bL, dL, eL), fL, hL) -
        AMin3F1(AMin3F1(bL, dL, eL), fL, hL)));
    nz = -0.5 * nz + 1.0;

    float mn4R = min(AMin3F1(bR, dR, fR), hR);
    float mn4G = min(AMin3F1(bG, dG, fG), hG);
    float mn4B = min(AMin3F1(bB, dB, fB), hB);
    float mx4R = max(AMax3F1(bR, dR, fR), hR);
    float mx4G = max(AMax3F1(bG, dG, fG), hG);
    float mx4B = max(AMax3F1(bB, dB, fB), hB);

    vec2 peakC = vec2(1.0, -4.0);
    float hitMinR = min(mn4R, eR) * ARcpF1(4.0 * mx4R);
    float hitMinG = min(mn4G, eG) * ARcpF1(4.0 * mx4G);
    float hitMinB = min(mn4B, eB) * ARcpF1(4.0 * mx4B);
    float hitMaxR = (peakC.x - max(mx4R, eR)) * ARcpF1(4.0 * mn4R + peakC.y);
    float hitMaxG = (peakC.x - max(mx4G, eG)) * ARcpF1(4.0 * mn4G + peakC.y);
    float hitMaxB = (peakC.x - max(mx4B, eB)) * ARcpF1(4.0 * mn4B + peakC.y);
    float lobeR = max(-hitMinR, hitMaxR);
    float lobeG = max(-hitMinG, hitMaxG);
    float lobeB = max(-hitMinB, hitMaxB);
    float lobe = max(-FSR_RCAS_LIMIT,
                     min(AMax3F1(lobeR, lobeG, lobeB), 0.0)) *
                 uintBitsToFloat(con.x);

    lobe *= nz;

    float rcpL = APrxMedRcpF1(4.0 * lobe + 1.0);
    pixR = (lobe * bR + lobe * dR + lobe * hR + lobe * fR + eR) * rcpL;
    pixG = (lobe * bG + lobe * dG + lobe * hG + lobe * fG + eG) * rcpL;
    pixB = (lobe * bB + lobe * dB + lobe * hB + lobe * fB + eB) * rcpL;
}

void main()
{
    vec3 color;
    FsrRcasF(color.r, color.g, color.b, uvec2(gl_FragCoord.xy), u.control);
    outColor = vec4(color, 1.0);
}