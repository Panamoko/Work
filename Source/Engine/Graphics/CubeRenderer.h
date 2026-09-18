#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

namespace uel {

    // 最小の立方体描画（法線ライティング）。world/viewProj/color を受けて1個描く。
    class CubeRenderer {
    public:
        bool Initialize(ID3D11Device* device);
        void Draw(ID3D11DeviceContext* context,
            const DirectX::XMFLOAT4X4& world,
            const DirectX::XMFLOAT4X4& viewProj,
            const DirectX::XMFLOAT4& color);

    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader>    vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>     ps;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>     layout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>          vb;
        Microsoft::WRL::ComPtr<ID3D11Buffer>          ib;
        Microsoft::WRL::ComPtr<ID3D11Buffer>          cb;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> raster;
        UINT indexCount = 0;
    };

} // namespace uel