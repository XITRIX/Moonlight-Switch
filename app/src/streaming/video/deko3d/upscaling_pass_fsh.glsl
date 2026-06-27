#version 460

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D inputTexture;

layout (std140, binding = 0) uniform PostProcessSettings
{
    vec4 control;
} u;

float interleavedGradientNoise(vec2 uv)
{
    return fract(52.9829189 * fract(dot(uv, vec2(0.06711056, 0.00583715))));
}

vec3 applyDither(vec3 rgb, float strength)
{
    rgb += (strength / 255.0) * interleavedGradientNoise(gl_FragCoord.xy) -
           (strength / 510.0);
    return clamp(rgb, 0.0, 1.0);
}

void main()
{
    vec3 rgb = texture(inputTexture, vTextureCoord).rgb;

    if (u.control.x > 0.5) {
        rgb = applyDither(rgb, u.control.y);
    }

    outColor = vec4(rgb, 1.0);
}