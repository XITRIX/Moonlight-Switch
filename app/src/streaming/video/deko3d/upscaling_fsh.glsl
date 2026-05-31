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

float fastApproxReciprocal(float value)
{
    return uintBitsToFloat(0x7ef07ebbu - floatBitsToUint(value));
}

float fastApproxRsqrt(float value)
{
    return uintBitsToFloat(0x5f347d74u - (floatBitsToUint(value) >> 1u));
}

void accumulateGradient(
    inout vec2 direction,
    inout float lengthAccumulator,
    float weight,
    float sampleA,
    float sampleB,
    float sampleC,
    float sampleD,
    float sampleE)
{
    float gradientX0 = sampleD - sampleC;
    float gradientX1 = sampleC - sampleB;
    float gradientScaleX = max(abs(gradientX0), abs(gradientX1));
    gradientScaleX = fastApproxReciprocal(max(gradientScaleX, 1e-4));

    float directionX = sampleD - sampleB;
    direction.x += directionX * weight;

    float gradientLengthX = clamp(abs(directionX) * gradientScaleX, 0.0, 1.0);
    lengthAccumulator += gradientLengthX * gradientLengthX * weight;

    float gradientY0 = sampleE - sampleC;
    float gradientY1 = sampleC - sampleA;
    float gradientScaleY = max(abs(gradientY0), abs(gradientY1));
    gradientScaleY = fastApproxReciprocal(max(gradientScaleY, 1e-4));

    float directionY = sampleE - sampleA;
    direction.y += directionY * weight;

    float gradientLengthY = clamp(abs(directionY) * gradientScaleY, 0.0, 1.0);
    lengthAccumulator += gradientLengthY * gradientLengthY * weight;
}

void accumulateTap(
    inout float accumulatedColor,
    inout float accumulatedWeight,
    vec2 offsetFromPixel,
    vec2 direction,
    vec2 anisotropy,
    float lobe,
    float clipRadius,
    float sampleValue)
{
    vec2 rotatedOffset;
    rotatedOffset.x = offsetFromPixel.x * direction.x + offsetFromPixel.y * direction.y;
    rotatedOffset.y = offsetFromPixel.x * (-direction.y) + offsetFromPixel.y * direction.x;
    rotatedOffset *= anisotropy;

    float distanceSquared = min(dot(rotatedOffset, rotatedOffset), clipRadius);
    float lanczosCore = ((2.0 / 5.0) * distanceSquared) - 1.0;
    float lanczosLobe = lobe * distanceSquared - 1.0;

    lanczosCore *= lanczosCore;
    lanczosLobe *= lanczosLobe;

    float kernelWeight = ((25.0 / 16.0) * lanczosCore - (25.0 / 16.0 - 1.0)) * lanczosLobe;
    accumulatedColor += sampleValue * kernelWeight;
    accumulatedWeight += kernelWeight;
}

float reconstructLuma(vec2 uv)
{
    vec2 sourceSize = vec2(textureSize(plane0, 0));
    vec2 invSourceSize = 1.0 / sourceSize;
    vec2 sourcePos = uv * sourceSize - vec2(0.5);
    vec2 basePos = floor(sourcePos);
    vec2 fracPos = sourcePos - basePos;

    vec4 gatherA = textureGather(plane0, basePos * invSourceSize);
    vec4 gatherB = textureGather(plane0, (basePos + vec2(2.0, 0.0)) * invSourceSize);
    vec4 gatherC = textureGather(plane0, (basePos + vec2(0.0, 2.0)) * invSourceSize);
    vec4 gatherD = textureGather(plane0, (basePos + vec2(2.0, 2.0)) * invSourceSize);

    float sampleB = gatherA.z;
    float sampleC = gatherB.w;
    float sampleE = gatherA.x;
    float sampleF = gatherA.y;
    float sampleG = gatherB.x;
    float sampleH = gatherB.y;
    float sampleI = gatherC.w;
    float sampleJ = gatherC.z;
    float sampleK = gatherD.w;
    float sampleL = gatherD.z;
    float sampleN = gatherC.y;
    float sampleO = gatherD.x;

    float minCenter = min(min(sampleF, sampleG), min(sampleJ, sampleK));
    float maxCenter = max(max(sampleF, sampleG), max(sampleJ, sampleK));
    float centerRange = maxCenter - minCenter;

    if (centerRange < (8.0 / 255.0)) {
        float top = mix(sampleF, sampleG, fracPos.x);
        float bottom = mix(sampleJ, sampleK, fracPos.x);
        return mix(top, bottom, fracPos.y);
    }

    vec2 direction = vec2(0.0);
    float lengthAccumulator = 0.0;

    accumulateGradient(direction, lengthAccumulator,
                       (1.0 - fracPos.x) * (1.0 - fracPos.y),
                       sampleB, sampleE, sampleF, sampleG, sampleJ);
    accumulateGradient(direction, lengthAccumulator,
                       fracPos.x * (1.0 - fracPos.y),
                       sampleC, sampleF, sampleG, sampleH, sampleK);
    accumulateGradient(direction, lengthAccumulator,
                       (1.0 - fracPos.x) * fracPos.y,
                       sampleF, sampleI, sampleJ, sampleK, sampleN);
    accumulateGradient(direction, lengthAccumulator,
                       fracPos.x * fracPos.y,
                       sampleG, sampleJ, sampleK, sampleL, sampleO);

    float directionMagnitudeSquared = dot(direction, direction);
    bool useFallbackDirection = directionMagnitudeSquared < (1.0 / 32768.0);
    float invDirectionMagnitude = useFallbackDirection ? 1.0 : fastApproxRsqrt(directionMagnitudeSquared);
    vec2 normalizedDirection = useFallbackDirection ? vec2(1.0, 0.0)
                                                    : direction * invDirectionMagnitude;

    float edgeConfidence = 0.5 * lengthAccumulator;
    edgeConfidence *= edgeConfidence;

    float stretch = dot(normalizedDirection, normalizedDirection) *
                    fastApproxReciprocal(max(max(abs(normalizedDirection.x), abs(normalizedDirection.y)), 1e-4));
    vec2 anisotropy = vec2(1.0 + (stretch - 1.0) * edgeConfidence,
                           1.0 - 0.5 * edgeConfidence);

    float lobe = 0.5 + ((0.25 - 0.04) - 0.5) * edgeConfidence;
    float clipRadius = fastApproxReciprocal(max(lobe, 1e-4));

    float accumulatedColor = 0.0;
    float accumulatedWeight = 0.0;

    accumulateTap(accumulatedColor, accumulatedWeight, vec2(0.0, -1.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleB);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(1.0, -1.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleC);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(-1.0, 1.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleI);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(0.0, 1.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleJ);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(0.0, 0.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleF);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(-1.0, 0.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleE);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(1.0, 1.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleK);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(2.0, 1.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleL);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(2.0, 0.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleH);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(1.0, 0.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleG);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(1.0, 2.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleO);
    accumulateTap(accumulatedColor, accumulatedWeight, vec2(0.0, 2.0) - fracPos,
                  normalizedDirection, anisotropy, lobe, clipRadius, sampleN);

    float reconstructed = accumulatedColor / max(accumulatedWeight, 1e-5);
    return clamp(reconstructed, minCenter, maxCenter);
}

void main()
{
    vec2 uv = (vTextureCoord - u.uv_data.xy) * u.uv_data.zw;

    float luma = reconstructLuma(uv);
    vec2 chroma = texture(plane1, uv).rg;
    vec3 yuv = vec3(luma, chroma) - u.offset;
    vec3 rgb = u.yuvmat * yuv;

    outColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}