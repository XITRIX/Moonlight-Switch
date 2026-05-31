#version 460

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D inputTexture;

void main()
{
    outColor = texture(inputTexture, vTextureCoord);
}