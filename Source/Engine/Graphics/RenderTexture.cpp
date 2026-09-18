#include "Engine/Graphics/RenderTexture.h"

using Microsoft::WRL::ComPtr;

namespace uel {

    bool RenderTexture::Create(ID3D11Device* device, int width, int height)
    {
        if (width <= 0 || height <= 0) return false;
        this->width = width; this->height = height;

        texture.Reset(); rtv.Reset(); srv.Reset(); depthTex.Reset(); dsv.Reset();

        // ƒJƒ‰[
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = width; td.Height = height; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&td, nullptr, texture.GetAddressOf())))              return false;
        if (FAILED(device->CreateRenderTargetView(texture.Get(), nullptr, rtv.GetAddressOf())))   return false;
        if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf()))) return false;

        // [“x
        D3D11_TEXTURE2D_DESC dd = {};
        dd.Width = width; dd.Height = height; dd.MipLevels = 1; dd.ArraySize = 1;
        dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; dd.SampleDesc.Count = 1;
        dd.Usage = D3D11_USAGE_DEFAULT; dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (FAILED(device->CreateTexture2D(&dd, nullptr, depthTex.GetAddressOf())))          return false;
        if (FAILED(device->CreateDepthStencilView(depthTex.Get(), nullptr, dsv.GetAddressOf()))) return false;

        return true;
    }

    void RenderTexture::Resize(ID3D11Device* device, int width, int height)
    {
        if (width <= 0 || height <= 0) return;
        if (width == this->width && height == this->height) return;
        Create(device, width, height);
    }

    void RenderTexture::Begin(ID3D11DeviceContext* context, float r, float g, float b, float a)
    {
        const float color[4] = { r, g, b, a };
        context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
        context->ClearRenderTargetView(rtv.Get(), color);
        context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        D3D11_VIEWPORT vp = {};
        vp.Width = (float)width; vp.Height = (float)height;
        vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);
    }

} // namespace uel