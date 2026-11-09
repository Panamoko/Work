#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Engine/Graphics/SceneLighting.h"

namespace uel {
    class EngineSkybox {
    public:
        bool Initialize(ID3D11Device* device);
        // skyCube：空の画像（キューブマップ）。nullptr ならグラデーションの空を描く
        // lighting：空の明るさ（skyExposure）と回転（skyRotation）を使う
        void Render(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewProj, const DirectX::XMFLOAT3& camPos,
            ID3D11ShaderResourceView* skyCube, const SceneLighting& lighting);
    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader>      vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       ps;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            cb;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> noDepth;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   raster;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>      sampler;   // 空の画像を読む（線形・端で止める）
    };
}