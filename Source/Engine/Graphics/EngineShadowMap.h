#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>

namespace uel {
    class Mesh;
    class SkinnedRenderModel;

    class EngineShadowMap {
    public:
        bool Initialize(ID3D11Device* device, int size = 2048);
        void BeginPass(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& lightViewProj);
        void DrawMesh(ID3D11DeviceContext* context, const Mesh& mesh, const DirectX::XMFLOAT4X4& world);
        void DrawSkinned(ID3D11DeviceContext* context, const SkinnedRenderModel& model,
            const DirectX::XMFLOAT4X4& world, const std::vector<DirectX::XMFLOAT4X4>& palette);
        ID3D11ShaderResourceView* GetSRV() const { return srv.Get(); }

    private:
        int size = 2048;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>          tex;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   dsv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>       vs;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>        layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>             cb;
        // スキンドメッシュ用（ボーンで変形してから深度を書く）
        Microsoft::WRL::ComPtr<ID3D11VertexShader>       skinnedVS;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>        skinnedLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>             boneCB;
        DirectX::XMFLOAT4X4 lightVP{};
    };
}