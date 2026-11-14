#version 450
layout(location = 0) out vec2 position;
void main()
{
    position    = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
