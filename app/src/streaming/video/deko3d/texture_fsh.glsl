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

void main()
{
    // Not work
    // vec2 uv = (vTextureCoord - u.uv_data.xy) * u.uv_data.zw;
    // vec3 YCbCr = vec3(texture(plane0, uv).r, texture(plane1, uv).r, texture(plane1, uv).g) - u.offset;
    // outColor = vec4(clamp(u.yuvmat * YCbCr, 0.0, 1.0), 1.0);

    // Almost work
    // vec3 YCbCr = vec3(
	// 	texture2D(plane0, vTextureCoord)[0],
	// 	texture2D(plane1, vTextureCoord).xy
	// );

	// YCbCr -= u.offset;
	// outColor = vec4(clamp(u.yuvmat * YCbCr, 0.0, 1.0), 1.0f);

    float r, g, b, yt, ut, vt;
    
    yt = texture2D(plane0, vTextureCoord).r;
    ut = texture2D(plane1, vTextureCoord).r - 0.5;// - u.offset.y;
    vt = texture2D(plane1, vTextureCoord).g - 0.5;// - u.offset.z;

    r = yt + 1.13983*vt;
    g = yt - 0.39465*ut - 0.58060*vt;
    b = yt + 2.03211*ut;

    outColor = vec4(r, g, b, 1.0);
}
