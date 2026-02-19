#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUv;

layout (location = 0) out vec2 vTextureCoord;

void main()
{
    gl_Position = vec4(inPos, 1.0);
    vTextureCoord = inUv;
}
