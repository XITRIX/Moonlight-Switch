#pragma once

#ifdef USE_D3D11_RENDERER

#include "IVideoRenderer.hpp"

#include <d3d11.h>

namespace brls
{
class D3D11Context;
}

class D3D11VideoRenderer : public IVideoRenderer {
  public:
  struct PlaneDesc {
    int widthDivisor;
    int heightDivisor;
    DXGI_FORMAT format;
    UINT bytesPerTexel;
  };

  struct ShaderConstants {
    float yuvRow0[4];
    float yuvRow1[4];
    float yuvRow2[4];
    float offset[4];
    float uvData[4];
  };

    D3D11VideoRenderer();
    ~D3D11VideoRenderer();

    void draw(NVGcontext* vg, int width, int height, AVFrame* frame, int imageFormat) override;
    VideoRenderStats* video_render_stats() override;

  private:
    static constexpr int MAX_VIDEO_PLANES = 3;

    enum class ShaderVariant {
        Unknown,
        TwoPlane,
        ThreePlane,
      TwoPlaneArray,
    };

    bool ensureContext();
    bool initialize();
    void cleanup();
    bool configureFrameFormat(int frameFormat);
    bool isHardwareFrame(const AVFrame* frame) const;
    bool resolveFrameFormat(const AVFrame* frame, int* frameFormat) const;
    bool ensureFrameResources(int width, int height, const AVFrame* frame);
    bool recreateTextures();
    void releaseTextures();
    bool createHardwarePlaneViews(const AVFrame* frame, ID3D11ShaderResourceView** planeViews,
      ID3D11PixelShader** pixelShader);
    void updateConstants(int width, int height, const AVFrame* frame);
    void uploadPlane(int planeIndex, const AVFrame* frame);

    brls::D3D11Context* m_d3d11Context = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_deviceContext = nullptr;

    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_twoPlaneShader = nullptr;
    ID3D11PixelShader* m_threePlaneShader = nullptr;
    ID3D11PixelShader* m_twoPlaneArrayShader = nullptr;
    ID3D11InputLayout* m_inputLayout = nullptr;
    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_constantsBuffer = nullptr;
    ID3D11SamplerState* m_samplerState = nullptr;
    ID3D11RasterizerState* m_rasterizerState = nullptr;
    ID3D11DepthStencilState* m_depthStencilState = nullptr;
    ID3D11BlendState* m_blendState = nullptr;

    ID3D11Texture2D* m_textures[MAX_VIDEO_PLANES] = {nullptr, nullptr, nullptr};
    ID3D11ShaderResourceView* m_shaderResourceViews[MAX_VIDEO_PLANES] = {nullptr, nullptr, nullptr};

    bool m_isInitialized = false;
    int m_frameFormat = AV_PIX_FMT_NONE;
    int m_frameWidth = 0;
    int m_frameHeight = 0;
    int m_screenWidth = 0;
    int m_screenHeight = 0;
    int m_textureWidths[MAX_VIDEO_PLANES] = {0, 0, 0};
    int m_textureHeights[MAX_VIDEO_PLANES] = {0, 0, 0};
    int m_planeCount = 0;
    bool m_usingHardwareFrame = false;
    ShaderVariant m_shaderVariant = ShaderVariant::Unknown;
    const PlaneDesc* m_currentPlanes = nullptr;

    VideoRenderStats m_videoRenderStatsProgress = {};
    VideoRenderStats m_videoRenderStatsCache = {};
    uint64_t m_statsTimeAccumulator = 0;
};

#endif // USE_D3D11_RENDERER