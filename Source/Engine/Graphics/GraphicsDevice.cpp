#include "Engine/Graphics/GraphicsDevice.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace uel {

    bool GraphicsDevice::Initialize(HWND hwnd, int width, int height)
    {
        this->width = width;
        this->height = height;

        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 2;
        scd.BufferDesc.Width = width;
        scd.BufferDesc.Height = height;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.RefreshRate.Numerator = 60;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = hwnd;
        scd.SampleDesc.Count = 1;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT flags = 0;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevel = {};
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            nullptr, 0, D3D11_SDK_VERSION,
            &scd, swapChain.GetAddressOf(), device.GetAddressOf(),
            &featureLevel, context.GetAddressOf());

#if defined(_DEBUG)
        // Graphics Tools（デバッグレイヤ）が無い環境向けフォールバック。
        if (FAILED(hr)) {
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                nullptr, 0, D3D11_SDK_VERSION,
                &scd, swapChain.GetAddressOf(), device.GetAddressOf(),
                &featureLevel, context.GetAddressOf());
        }
#endif
        if (FAILED(hr)) return false;

        // バックバッファ -> RTV
        ComPtr<ID3D11Texture2D> backBuffer;
        hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
        if (FAILED(hr)) return false;
        hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.GetAddressOf());
        if (FAILED(hr)) return false;

        // ビューポート
        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);

        return true;
    }

    void GraphicsDevice::BeginFrame(float r, float g, float b, float a)
    {
        const float color[4] = { r, g, b, a };
        context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
        context->ClearRenderTargetView(renderTargetView.Get(), color);
    }

    void GraphicsDevice::EndFrame()
    {
        swapChain->Present(1, 0); // VSync ON
    }

} // namespace uel