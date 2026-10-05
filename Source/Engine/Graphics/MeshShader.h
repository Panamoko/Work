#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector> 
#include <cstdint>

#include "Engine/Graphics/SceneLighting.h"
#include "Engine/Graphics/ShadingParams.h"
namespace uel {
    class Mesh;

    // 法線ライティングで Mesh を描く共通シェーダ。
    class MeshShader {
    public:
        bool Initialize(ID3D11Device* device);

        void Draw(ID3D11DeviceContext* context, const Mesh& mesh,
            const DirectX::XMFLOAT4X4& world,
            const DirectX::XMFLOAT4X4& viewProj,
            const DirectX::XMFLOAT4& color,
            const ShadingParams& shading,
            const std::vector<uint8_t>* hiddenSubmesh = nullptr,
            const std::vector<uint8_t>* submeshRole = nullptr,
            bool firstPerson = false);


        void SetLight(const SceneLighting& light);
        void SetShadow(ID3D11ShaderResourceView* srv, const DirectX::XMFLOAT4X4& lightViewProj);
        void SetCameraPosition(const DirectX::XMFLOAT3& pos) { cameraPos = pos; }

    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader>    vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>     ps;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>     layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>          cb;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> raster;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>       sampler;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteSRV;

        DirectX::XMFLOAT3 lightDir{ -0.4f, -1.0f, 0.35f };
        DirectX::XMFLOAT3 lightColor{ 1.0f,  1.0f, 1.0f };
        float             lightAmbient = 0.25f;
        float             lightIntensity = 1.0f;

        Microsoft::WRL::ComPtr<ID3D11SamplerState>       shadowSampler;
        ID3D11ShaderResourceView* shadowSRV = nullptr;   // 非所有
        DirectX::XMFLOAT4X4 lightViewProj{};
        bool shadowEnabled = false;
        DirectX::XMFLOAT3 cameraPos{ 0.0f, 0.0f, 0.0f };

        // アウトライン用（ふくらませた形の裏面だけを描く）
        Microsoft::WRL::ComPtr<ID3D11VertexShader>    vsOutline;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>     psOutline;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterOutline;
    };

} // namespace uel