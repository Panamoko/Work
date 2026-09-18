#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace uel {

    class RenderTexture {
    public:
        bool Create(ID3D11Device* device, int width, int height);
        void Resize(ID3D11Device* device, int width, int height);
        void Begin(ID3D11DeviceContext* context, float r, float g, float b, float a); // RTV+DSVをバインド＆クリア

        ID3D11ShaderResourceView* GetSRV() const { return srv.Get(); }
        int GetWidth()  const { return width; }
        int GetHeight() const { return height; }

    private:
        Microsoft::WRL::ComPtr<ID3D11Texture2D>          texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>          depthTex;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   dsv;
        int width = 0;
        int height = 0;
    };

} // namespace uel