#version 460

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec4 inAttrib;

layout (location = 0) out vec4 outAttrib;

void main()
{
    gl_Position = vec4(inPos, 1.0);
    outAttrib = inAttrib;
}
