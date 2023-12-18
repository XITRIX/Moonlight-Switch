#version 460

layout (location = 0) in vec2 vTextureCoord;
layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D sTextureY;
layout (binding = 1) uniform sampler2D sTextureUV;

void main()
{
    float r, g, b, y, u, v;
    y = texture2D(sTextureY, vTextureCoord).r;
    u = texture2D(sTextureUV, vTextureCoord).r - 0.5;
    v = texture2D(sTextureUV, vTextureCoord).g - 0.5;

    r = y;// + 1.13983*v;
    g = y;// - 0.39465*u - 0.58060*v;
    b = y;// + 2.03211*u;

    outColor = vec4(r, g, b, 1.0);//texture(texture0, inTexCoord);
    // outColor = texture(sTextureUV, vTextureCoord / 2);
}
