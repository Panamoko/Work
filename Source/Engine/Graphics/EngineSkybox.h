#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

namespace uel {
    class EngineSkybox {
    public:
        bool Initialize(ID3D11Device* device);
        void Render(ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& viewProj, const DirectX::XMFLOAT3& camPos);
    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader>      vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       ps;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            cb;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> noDepth;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   raster;
    };
}