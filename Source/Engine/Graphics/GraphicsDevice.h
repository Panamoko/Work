#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace uel {

    // D3D11 の device/context/swapchain/RTV を隔離するバックエンド。
    // 上位層は D3D を直接触らず、Begin/End だけを使う。
    class GraphicsDevice {
    public:
        bool Initialize(HWND hwnd, int width, int height);

        void BeginFrame(float r, float g, float b, float a); // RTVをセットしてクリア
        void EndFrame();                                     // Present

        ID3D11Device* GetDevice()  const { return device.Get(); }
        ID3D11DeviceContext* GetContext() const { return context.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D11Device>           device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>    context;
        Microsoft::WRL::ComPtr<IDXGISwapChain>         swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
        int width = 0;
        int height = 0;
    };

} // namespace uel