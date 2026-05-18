#ifdef USE_D3D11_RENDERER

#include "D3D11VideoRenderer.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include <d3dcompiler.h>

#include <borealis.hpp>
#include <borealis/platforms/driver/d3d11.hpp>
#include <borealis/platforms/sdl/sdl_video.hpp>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

namespace
{

using PlaneDesc = D3D11VideoRenderer::PlaneDesc;
using ShaderConstants = D3D11VideoRenderer::ShaderConstants;

struct Vertex {
    float position[2];
};

constexpr std::array<Vertex, 4> FullscreenVertices = {
    Vertex{{-1.0f, -1.0f}},
    Vertex{{1.0f, -1.0f}},
    Vertex{{-1.0f, 1.0f}},
    Vertex{{1.0f, 1.0f}},
};

constexpr PlaneDesc Nv12Planes[] = {
    {1, 1, DXGI_FORMAT_R8_UNORM, 1},
    {2, 2, DXGI_FORMAT_R8G8_UNORM, 2},
    {0, 0, DXGI_FORMAT_UNKNOWN, 0},
};

constexpr PlaneDesc Yuv420Planes[] = {
    {1, 1, DXGI_FORMAT_R8_UNORM, 1},
    {2, 2, DXGI_FORMAT_R8_UNORM, 1},
    {2, 2, DXGI_FORMAT_R8_UNORM, 1},
};

constexpr PlaneDesc P010Planes[] = {
    {1, 1, DXGI_FORMAT_R16_UNORM, 2},
    {2, 2, DXGI_FORMAT_R16G16_UNORM, 4},
    {0, 0, DXGI_FORMAT_UNKNOWN, 0},
};

constexpr UINT FullBlendMask = 0xffffffffu;

const char* VertexShaderSource = R"hlsl(
struct VSInput {
    float2 position : POSITION;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

PSInput main(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 0.0f, 1.0f);
    output.texCoord = float2(input.position.x * 0.5f + 0.5f,
                             0.5f - input.position.y * 0.5f);
    return output;
}
)hlsl";

const char* TwoPlanePixelShaderSource = R"hlsl(
Texture2D plane0 : register(t0);
Texture2D plane1 : register(t1);
SamplerState linearSampler : register(s0);

cbuffer VideoConstants : register(b0) {
    float4 yuvRow0;
    float4 yuvRow1;
    float4 yuvRow2;
    float4 offset;
    float4 uvData;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float2 uv = (input.texCoord - uvData.xy) * uvData.zw;
    float3 ycbcr = float3(plane0.Sample(linearSampler, uv).r,
                          plane1.Sample(linearSampler, uv).r,
                          plane1.Sample(linearSampler, uv).g) - offset.xyz;
    float3 rgb = float3(dot(yuvRow0.xyz, ycbcr),
                        dot(yuvRow1.xyz, ycbcr),
                        dot(yuvRow2.xyz, ycbcr));
    return float4(saturate(rgb), 1.0f);
}
)hlsl";

const char* ThreePlanePixelShaderSource = R"hlsl(
Texture2D plane0 : register(t0);
Texture2D plane1 : register(t1);
Texture2D plane2 : register(t2);
SamplerState linearSampler : register(s0);

cbuffer VideoConstants : register(b0) {
    float4 yuvRow0;
    float4 yuvRow1;
    float4 yuvRow2;
    float4 offset;
    float4 uvData;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float2 uv = (input.texCoord - uvData.xy) * uvData.zw;
    float3 ycbcr = float3(plane0.Sample(linearSampler, uv).r,
                          plane1.Sample(linearSampler, uv).r,
                          plane2.Sample(linearSampler, uv).r) - offset.xyz;
    float3 rgb = float3(dot(yuvRow0.xyz, ycbcr),
                        dot(yuvRow1.xyz, ycbcr),
                        dot(yuvRow2.xyz, ycbcr));
    return float4(saturate(rgb), 1.0f);
}
)hlsl";

const char* TwoPlaneArrayPixelShaderSource = R"hlsl(
Texture2DArray plane0 : register(t0);
Texture2DArray plane1 : register(t1);
SamplerState linearSampler : register(s0);

cbuffer VideoConstants : register(b0) {
    float4 yuvRow0;
    float4 yuvRow1;
    float4 yuvRow2;
    float4 offset;
    float4 uvData;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float2 uv = (input.texCoord - uvData.xy) * uvData.zw;
    float3 coord = float3(uv, 0.0f);
    float3 ycbcr = float3(plane0.Sample(linearSampler, coord).r,
                          plane1.Sample(linearSampler, coord).r,
                          plane1.Sample(linearSampler, coord).g) - offset.xyz;
    float3 rgb = float3(dot(yuvRow0.xyz, ycbcr),
                        dot(yuvRow1.xyz, ycbcr),
                        dot(yuvRow2.xyz, ycbcr));
    return float4(saturate(rgb), 1.0f);
}
)hlsl";

template <typename T>
void safeRelease(T*& object)
{
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

const float* colorOffset(bool colorFull)
{
    static const float LimitedOffsets[] = {16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f};
    static const float FullOffsets[] = {0.0f, 128.0f / 255.0f, 128.0f / 255.0f};
    return colorFull ? FullOffsets : LimitedOffsets;
}

const float* colorMatrix(enum AVColorSpace colorSpace, bool colorFull)
{
    static const float Bt601Limited[] = {
        1.1644f, 1.1644f, 1.1644f,
        0.0f, -0.3917f, 2.0172f,
        1.5960f, -0.8129f, 0.0f,
    };
    static const float Bt601Full[] = {
        1.0f, 1.0f, 1.0f,
        0.0f, -0.3441f, 1.7720f,
        1.4020f, -0.7141f, 0.0f,
    };
    static const float Bt709Limited[] = {
        1.1644f, 1.1644f, 1.1644f,
        0.0f, -0.2132f, 2.1124f,
        1.7927f, -0.5329f, 0.0f,
    };
    static const float Bt709Full[] = {
        1.0f, 1.0f, 1.0f,
        0.0f, -0.1873f, 1.8556f,
        1.5748f, -0.4681f, 0.0f,
    };
    static const float Bt2020Limited[] = {
        1.1644f, 1.1644f, 1.1644f,
        0.0f, -0.1874f, 2.1418f,
        1.6781f, -0.6505f, 0.0f,
    };
    static const float Bt2020Full[] = {
        1.0f, 1.0f, 1.0f,
        0.0f, -0.1646f, 1.8814f,
        1.4746f, -0.5714f, 0.0f,
    };

    switch (colorSpace) {
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_BT470BG:
            return colorFull ? Bt601Full : Bt601Limited;
        case AVCOL_SPC_BT709:
            return colorFull ? Bt709Full : Bt709Limited;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL:
            return colorFull ? Bt2020Full : Bt2020Limited;
        default:
            return Bt601Limited;
    }
}

void fillShaderMatrixRows(const float* matrix, ShaderConstants& constants)
{
    constants.yuvRow0[0] = matrix[0];
    constants.yuvRow0[1] = matrix[3];
    constants.yuvRow0[2] = matrix[6];
    constants.yuvRow0[3] = 0.0f;

    constants.yuvRow1[0] = matrix[1];
    constants.yuvRow1[1] = matrix[4];
    constants.yuvRow1[2] = matrix[7];
    constants.yuvRow1[3] = 0.0f;

    constants.yuvRow2[0] = matrix[2];
    constants.yuvRow2[1] = matrix[5];
    constants.yuvRow2[2] = matrix[8];
    constants.yuvRow2[3] = 0.0f;
}

HRESULT compileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** shaderBlob)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob* errors = nullptr;
    const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
        entryPoint, target, flags, 0, shaderBlob, &errors);

    if (FAILED(hr) && errors != nullptr) {
        brls::Logger::error("D3D11VideoRenderer: shader compile failed - {}",
            static_cast<const char*>(errors->GetBufferPointer()));
    }

    safeRelease(errors);
    return hr;
}

} // namespace

D3D11VideoRenderer::D3D11VideoRenderer() = default;

D3D11VideoRenderer::~D3D11VideoRenderer()
{
    cleanup();
}

bool D3D11VideoRenderer::ensureContext()
{
    if (m_device != nullptr && m_deviceContext != nullptr) {
        return true;
    }

    auto* videoContext = static_cast<brls::SDLVideoContext*>(brls::Application::getPlatform()->getVideoContext());
    if (videoContext == nullptr) {
        brls::Logger::error("D3D11VideoRenderer: SDL video context is unavailable");
        return false;
    }

    m_d3d11Context = videoContext->getD3D11Context();
    if (m_d3d11Context == nullptr) {
        brls::Logger::error("D3D11VideoRenderer: Borealis D3D11 context is unavailable");
        return false;
    }

    m_device = m_d3d11Context->getDevice();
    m_deviceContext = m_d3d11Context->getDeviceContext();

    if (m_device == nullptr || m_deviceContext == nullptr) {
        brls::Logger::error("D3D11VideoRenderer: failed to acquire device or device context");
        return false;
    }

    return true;
}

bool D3D11VideoRenderer::initialize()
{
    if (m_isInitialized) {
        return true;
    }

    if (!ensureContext()) {
        return false;
    }

    ID3DBlob* vertexShaderBlob = nullptr;
    ID3DBlob* twoPlaneShaderBlob = nullptr;
    ID3DBlob* threePlaneShaderBlob = nullptr;
    ID3DBlob* twoPlaneArrayShaderBlob = nullptr;

    HRESULT hr = compileShader(VertexShaderSource, "main", "vs_4_0", &vertexShaderBlob);
    if (FAILED(hr)) {
        goto cleanup_shader_blobs;
    }

    hr = compileShader(TwoPlanePixelShaderSource, "main", "ps_4_0", &twoPlaneShaderBlob);
    if (FAILED(hr)) {
        goto cleanup_shader_blobs;
    }

    hr = compileShader(ThreePlanePixelShaderSource, "main", "ps_4_0", &threePlaneShaderBlob);
    if (FAILED(hr)) {
        goto cleanup_shader_blobs;
    }

    hr = compileShader(TwoPlaneArrayPixelShaderSource, "main", "ps_4_0", &twoPlaneArrayShaderBlob);
    if (FAILED(hr)) {
        goto cleanup_shader_blobs;
    }

    hr = m_device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) {
        brls::Logger::error("D3D11VideoRenderer: failed to create vertex shader");
        goto cleanup_shader_blobs;
    }

    hr = m_device->CreatePixelShader(twoPlaneShaderBlob->GetBufferPointer(),
        twoPlaneShaderBlob->GetBufferSize(), nullptr, &m_twoPlaneShader);
    if (FAILED(hr)) {
        brls::Logger::error("D3D11VideoRenderer: failed to create two-plane pixel shader");
        goto cleanup_shader_blobs;
    }

    hr = m_device->CreatePixelShader(threePlaneShaderBlob->GetBufferPointer(),
        threePlaneShaderBlob->GetBufferSize(), nullptr, &m_threePlaneShader);
    if (FAILED(hr)) {
        brls::Logger::error("D3D11VideoRenderer: failed to create three-plane pixel shader");
        goto cleanup_shader_blobs;
    }

    hr = m_device->CreatePixelShader(twoPlaneArrayShaderBlob->GetBufferPointer(),
        twoPlaneArrayShaderBlob->GetBufferSize(), nullptr, &m_twoPlaneArrayShader);
    if (FAILED(hr)) {
        brls::Logger::error("D3D11VideoRenderer: failed to create two-plane array pixel shader");
        goto cleanup_shader_blobs;
    }

    {
        const D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        hr = m_device->CreateInputLayout(inputDesc, 1,
            vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &m_inputLayout);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create input layout");
            goto cleanup_shader_blobs;
        }
    }

    {
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        bufferDesc.ByteWidth = static_cast<UINT>(sizeof(FullscreenVertices));
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data = {};
        data.pSysMem = FullscreenVertices.data();

        hr = m_device->CreateBuffer(&bufferDesc, &data, &m_vertexBuffer);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create vertex buffer");
            goto cleanup_shader_blobs;
        }
    }

    {
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.ByteWidth = sizeof(ShaderConstants);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = m_device->CreateBuffer(&bufferDesc, nullptr, &m_constantsBuffer);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create constants buffer");
            goto cleanup_shader_blobs;
        }
    }

    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = m_device->CreateSamplerState(&desc, &m_samplerState);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create sampler state");
            goto cleanup_shader_blobs;
        }
    }

    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.DepthClipEnable = TRUE;
        desc.ScissorEnable = FALSE;

        hr = m_device->CreateRasterizerState(&desc, &m_rasterizerState);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create rasterizer state");
            goto cleanup_shader_blobs;
        }
    }

    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = FALSE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        desc.StencilEnable = FALSE;

        hr = m_device->CreateDepthStencilState(&desc, &m_depthStencilState);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create depth stencil state");
            goto cleanup_shader_blobs;
        }
    }

    {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].BlendEnable = FALSE;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = m_device->CreateBlendState(&desc, &m_blendState);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create blend state");
            goto cleanup_shader_blobs;
        }
    }

    m_isInitialized = true;

cleanup_shader_blobs:
    safeRelease(vertexShaderBlob);
    safeRelease(twoPlaneShaderBlob);
    safeRelease(threePlaneShaderBlob);
    safeRelease(twoPlaneArrayShaderBlob);

    if (!m_isInitialized) {
        cleanup();
    }

    return m_isInitialized;
}

void D3D11VideoRenderer::cleanup()
{
    releaseTextures();

    safeRelease(m_blendState);
    safeRelease(m_depthStencilState);
    safeRelease(m_rasterizerState);
    safeRelease(m_samplerState);
    safeRelease(m_constantsBuffer);
    safeRelease(m_vertexBuffer);
    safeRelease(m_inputLayout);
    safeRelease(m_twoPlaneShader);
    safeRelease(m_threePlaneShader);
    safeRelease(m_twoPlaneArrayShader);
    safeRelease(m_vertexShader);

    m_isInitialized = false;
    m_device = nullptr;
    m_deviceContext = nullptr;
    m_d3d11Context = nullptr;
    m_frameFormat = AV_PIX_FMT_NONE;
    m_frameWidth = 0;
    m_frameHeight = 0;
    m_screenWidth = 0;
    m_screenHeight = 0;
    m_planeCount = 0;
    m_usingHardwareFrame = false;
    m_shaderVariant = ShaderVariant::Unknown;
    m_currentPlanes = nullptr;
}

bool D3D11VideoRenderer::configureFrameFormat(int frameFormat)
{
    switch (frameFormat) {
        case AV_PIX_FMT_YUV420P:
            m_currentPlanes = Yuv420Planes;
            m_planeCount = 3;
            m_shaderVariant = ShaderVariant::ThreePlane;
            return true;
        case AV_PIX_FMT_NV12:
            m_currentPlanes = Nv12Planes;
            m_planeCount = 2;
            m_shaderVariant = ShaderVariant::TwoPlane;
            return true;
        case AV_PIX_FMT_P010:
            m_currentPlanes = P010Planes;
            m_planeCount = 2;
            m_shaderVariant = ShaderVariant::TwoPlane;
            return true;
        default:
            brls::Logger::error("D3D11VideoRenderer: unsupported frame format {}", frameFormat);
            m_currentPlanes = nullptr;
            m_planeCount = 0;
            m_shaderVariant = ShaderVariant::Unknown;
            return false;
    }
}

bool D3D11VideoRenderer::isHardwareFrame(const AVFrame* frame) const
{
    return frame != nullptr && frame->format == AV_PIX_FMT_D3D11;
}

bool D3D11VideoRenderer::resolveFrameFormat(const AVFrame* frame, int* frameFormat) const
{
    if (frame == nullptr || frameFormat == nullptr) {
        return false;
    }

    if (!isHardwareFrame(frame)) {
        *frameFormat = frame->format;
        return true;
    }

    if (frame->hw_frames_ctx == nullptr) {
        brls::Logger::error("D3D11VideoRenderer: missing hw_frames_ctx for imported hardware frame");
        return false;
    }

    auto* framesContext = reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
    if (framesContext == nullptr) {
        brls::Logger::error("D3D11VideoRenderer: invalid D3D11 hardware frame context");
        return false;
    }

    *frameFormat = framesContext->sw_format;
    return true;
}

bool D3D11VideoRenderer::ensureFrameResources(int width, int height, const AVFrame* frame)
{
    const bool hardwareFrame = isHardwareFrame(frame);
    int resolvedFrameFormat = AV_PIX_FMT_NONE;
    if (!resolveFrameFormat(frame, &resolvedFrameFormat) || !configureFrameFormat(resolvedFrameFormat)) {
        return false;
    }

    const bool frameShapeChanged =
        m_frameFormat != resolvedFrameFormat ||
        m_frameWidth != frame->width ||
        m_frameHeight != frame->height;

    if (hardwareFrame) {
        if (!m_usingHardwareFrame || frameShapeChanged) {
            releaseTextures();
        }

        m_usingHardwareFrame = true;
        m_frameFormat = resolvedFrameFormat;
        m_frameWidth = frame->width;
        m_frameHeight = frame->height;

        m_screenWidth = width;
        m_screenHeight = height;
        return true;
    }

    const bool needsTextureRecreation = m_usingHardwareFrame || frameShapeChanged;

    if (needsTextureRecreation) {
        m_frameFormat = resolvedFrameFormat;
        m_frameWidth = frame->width;
        m_frameHeight = frame->height;
        if (!recreateTextures()) {
            return false;
        }
    }

    m_usingHardwareFrame = false;
    m_screenWidth = width;
    m_screenHeight = height;
    return true;
}

bool D3D11VideoRenderer::recreateTextures()
{
    releaseTextures();

    for (int index = 0; index < m_planeCount; index++) {
        m_textureWidths[index] = m_frameWidth / m_currentPlanes[index].widthDivisor;
        m_textureHeights[index] = m_frameHeight / m_currentPlanes[index].heightDivisor;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = static_cast<UINT>(m_textureWidths[index]);
        desc.Height = static_cast<UINT>(m_textureHeights[index]);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = m_currentPlanes[index].format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_textures[index]);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create plane {} texture", index);
            releaseTextures();
            return false;
        }

        hr = m_device->CreateShaderResourceView(m_textures[index], nullptr, &m_shaderResourceViews[index]);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create plane {} shader resource view", index);
            releaseTextures();
            return false;
        }
    }

    return true;
}

void D3D11VideoRenderer::releaseTextures()
{
    for (int index = 0; index < MAX_VIDEO_PLANES; index++) {
        safeRelease(m_shaderResourceViews[index]);
        safeRelease(m_textures[index]);
        m_textureWidths[index] = 0;
        m_textureHeights[index] = 0;
    }
}

bool D3D11VideoRenderer::createHardwarePlaneViews(const AVFrame* frame,
    ID3D11ShaderResourceView** planeViews, ID3D11PixelShader** pixelShader)
{
    if (frame == nullptr || planeViews == nullptr || pixelShader == nullptr) {
        return false;
    }

    int resolvedFrameFormat = AV_PIX_FMT_NONE;
    if (!resolveFrameFormat(frame, &resolvedFrameFormat)) {
        return false;
    }

    if (resolvedFrameFormat != AV_PIX_FMT_NV12 && resolvedFrameFormat != AV_PIX_FMT_P010) {
        brls::Logger::error("D3D11VideoRenderer: zero-copy requires NV12 or P010 D3D11 frames, got {}",
            resolvedFrameFormat);
        return false;
    }

    auto* texture = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
    if (texture == nullptr) {
        brls::Logger::error("D3D11VideoRenderer: imported frame is missing the D3D11 texture handle");
        return false;
    }

    const UINT arraySlice = static_cast<UINT>(reinterpret_cast<uintptr_t>(frame->data[1]));

    D3D11_TEXTURE2D_DESC textureDesc = {};
    texture->GetDesc(&textureDesc);

    const PlaneDesc* planeDescs = resolvedFrameFormat == AV_PIX_FMT_P010 ? P010Planes : Nv12Planes;
    const bool useArrayViews = textureDesc.ArraySize > 1;

    for (int index = 0; index < 2; index++) {
        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
        viewDesc.Format = planeDescs[index].format;

        if (useArrayViews) {
            viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            viewDesc.Texture2DArray.MostDetailedMip = 0;
            viewDesc.Texture2DArray.MipLevels = 1;
            viewDesc.Texture2DArray.FirstArraySlice = arraySlice;
            viewDesc.Texture2DArray.ArraySize = 1;
        } else {
            viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MostDetailedMip = 0;
            viewDesc.Texture2D.MipLevels = 1;
        }

        const HRESULT hr = m_device->CreateShaderResourceView(texture, &viewDesc, &planeViews[index]);
        if (FAILED(hr)) {
            brls::Logger::error("D3D11VideoRenderer: failed to create zero-copy shader resource view for plane {}",
                index);
            safeRelease(planeViews[0]);
            safeRelease(planeViews[1]);
            return false;
        }
    }

    *pixelShader = useArrayViews ? m_twoPlaneArrayShader : m_twoPlaneShader;
    return true;
}

void D3D11VideoRenderer::updateConstants(int width, int height, const AVFrame* frame)
{
    if (m_deviceContext == nullptr) {
        return;
    }

    const bool colorFull = frame->color_range == AVCOL_RANGE_JPEG;

    ShaderConstants constants = {};
    fillShaderMatrixRows(colorMatrix(frame->colorspace, colorFull), constants);

    const float* offsets = colorOffset(colorFull);
    constants.offset[0] = offsets[0];
    constants.offset[1] = offsets[1];
    constants.offset[2] = offsets[2];
    constants.offset[3] = 0.0f;

    const float frameAspect = static_cast<float>(frame->height) / static_cast<float>(std::max(frame->width, 1));
    const float screenAspect = static_cast<float>(std::max(height, 1)) / static_cast<float>(std::max(width, 1));

    if (frameAspect > screenAspect) {
        const float multiplier = frameAspect / screenAspect;
        constants.uvData[0] = 0.5f - 0.5f * (1.0f / multiplier);
        constants.uvData[1] = 0.0f;
        constants.uvData[2] = multiplier;
        constants.uvData[3] = 1.0f;
    } else {
        const float multiplier = screenAspect / frameAspect;
        constants.uvData[0] = 0.0f;
        constants.uvData[1] = 0.5f - 0.5f * (1.0f / multiplier);
        constants.uvData[2] = 1.0f;
        constants.uvData[3] = multiplier;
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_deviceContext->Map(m_constantsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        std::memcpy(mapped.pData, &constants, sizeof(constants));
        m_deviceContext->Unmap(m_constantsBuffer, 0);
    }
}

void D3D11VideoRenderer::uploadPlane(int planeIndex, const AVFrame* frame)
{
    if (planeIndex >= m_planeCount || m_textures[planeIndex] == nullptr || m_deviceContext == nullptr) {
        return;
    }

    const uint8_t* src = frame->data[planeIndex];
    const int srcPitch = frame->linesize[planeIndex];
    if (src == nullptr || srcPitch <= 0) {
        return;
    }

    m_deviceContext->UpdateSubresource(m_textures[planeIndex], 0, nullptr, src, static_cast<UINT>(srcPitch), 0);
}

void D3D11VideoRenderer::draw(NVGcontext* vg, int width, int height, AVFrame* frame, int imageFormat)
{
    (void)vg;
    (void)imageFormat;

    if (frame == nullptr) {
        return;
    }

    if (!m_videoRenderStatsProgress.rendered_frames) {
        m_videoRenderStatsProgress.measurement_start_timestamp = LiGetMillis();
    }

    const uint64_t beforeRender = LiGetMillis();

    if (!initialize() || !ensureFrameResources(width, height, frame)) {
        return;
    }

    const bool hardwareFrame = isHardwareFrame(frame);

    ID3D11ShaderResourceView* shaderResourceViews[MAX_VIDEO_PLANES] = {nullptr, nullptr, nullptr};
    ID3D11PixelShader* pixelShader = nullptr;

    if (hardwareFrame) {
        if (!createHardwarePlaneViews(frame, shaderResourceViews, &pixelShader)) {
            return;
        }
    } else {
        for (int index = 0; index < m_planeCount; index++) {
            uploadPlane(index, frame);
            shaderResourceViews[index] = m_shaderResourceViews[index];
        }

        pixelShader = m_shaderVariant == ShaderVariant::ThreePlane ? m_threePlaneShader : m_twoPlaneShader;
    }

    updateConstants(width, height, frame);

    constexpr UINT stride = sizeof(Vertex);
    constexpr UINT offset = 0;

    m_deviceContext->IASetInputLayout(m_inputLayout);
    m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_deviceContext->VSSetShader(m_vertexShader, nullptr, 0);
    m_deviceContext->PSSetShader(pixelShader, nullptr, 0);
    m_deviceContext->PSSetConstantBuffers(0, 1, &m_constantsBuffer);
    m_deviceContext->PSSetShaderResources(0, MAX_VIDEO_PLANES, shaderResourceViews);
    m_deviceContext->PSSetSamplers(0, 1, &m_samplerState);
    m_deviceContext->RSSetState(m_rasterizerState);
    m_deviceContext->OMSetDepthStencilState(m_depthStencilState, 0);
    m_deviceContext->OMSetBlendState(m_blendState, nullptr, FullBlendMask);
    m_deviceContext->Draw(4, 0);

    ID3D11ShaderResourceView* nullShaderResourceViews[MAX_VIDEO_PLANES] = {nullptr, nullptr, nullptr};
    m_deviceContext->PSSetShaderResources(0, MAX_VIDEO_PLANES, nullShaderResourceViews);

    if (hardwareFrame) {
        for (auto*& shaderResourceView : shaderResourceViews) {
            safeRelease(shaderResourceView);
        }
    }

    const uint64_t renderTime = LiGetMillis() - beforeRender;
    m_statsTimeAccumulator += renderTime;
    m_videoRenderStatsProgress.total_render_time += renderTime;
    m_videoRenderStatsProgress.rendered_frames++;

    const int statsIntervalMs = 200;
    if (m_statsTimeAccumulator >= statsIntervalMs) {
        m_videoRenderStatsCache = m_videoRenderStatsProgress;
        m_videoRenderStatsProgress = {};

        const uint64_t now = LiGetMillis();
        m_videoRenderStatsCache.rendered_fps = static_cast<float>(m_videoRenderStatsCache.rendered_frames) /
            (static_cast<float>(now - m_videoRenderStatsCache.measurement_start_timestamp) / 1000.0f);
        m_videoRenderStatsCache.rendering_time = static_cast<float>(m_videoRenderStatsCache.total_render_time) /
            static_cast<float>(std::max(m_videoRenderStatsCache.rendered_frames, 1u));

        m_statsTimeAccumulator -= statsIntervalMs;
    }
}

VideoRenderStats* D3D11VideoRenderer::video_render_stats()
{
    return &m_videoRenderStatsCache;
}

#endif // USE_D3D11_RENDERER