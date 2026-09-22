#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector> 
#include <cstdint>
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
            const std::vector<uint8_t>* hiddenSubmesh = nullptr,
            const std::vector<uint8_t>* submeshRole = nullptr,
            bool firstPerson = false);

    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader>    vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>     ps;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>     layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>          cb;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> raster;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>       sampler;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteSRV;
    };

} // namespace uel