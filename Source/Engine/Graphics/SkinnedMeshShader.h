#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>
#include "Engine/Graphics/SceneLighting.h"

namespace uel {
    class SkinnedRenderModel;

    // GPUスキニング＋ライティング（アンビエント＋平行光）＋影の受け取り。
    class SkinnedMeshShader {
    public:
        bool Initialize(ID3D11Device* device);
        void Draw(ID3D11DeviceContext* ctx, const SkinnedRenderModel& model,
            const DirectX::XMFLOAT4X4& world,
            const DirectX::XMFLOAT4X4& viewProj,
            const DirectX::XMFLOAT4& color,
            const std::vector<DirectX::XMFLOAT4X4>& palette);
        void SetLight(const SceneLighting& light);
        void SetShadow(ID3D11ShaderResourceView* srv, const DirectX::XMFLOAT4X4& lightViewProj);

    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader>       vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>        ps;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>        layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>             objCB, boneCB;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>    raster;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>       sampler;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>       shadowSampler;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteSRV;

        DirectX::XMFLOAT3 lightDir{ -0.4f, -1.0f, 0.35f };
        DirectX::XMFLOAT3 lightColor{ 1.0f, 1.0f, 1.0f };
        float lightAmbient = 0.25f;
        float lightIntensity = 1.0f;

        ID3D11ShaderResourceView* shadowSRV = nullptr;   // 非所有
        DirectX::XMFLOAT4X4 lightViewProj{};
        bool shadowEnabled = false;
    };

} // namespace uel