#include "Engine/Graphics/RenderTexture.h"

using Microsoft::WRL::ComPtr;

namespace uel {

    bool RenderTexture::Create(ID3D11Device* device, int width, int height, bool forScene)
    {
        if (width <= 0 || height <= 0) return false;
        this->width = width; this->height = height; this->forScene = forScene;

        texture.Reset(); rtv.Reset(); srv.Reset();
        normalTex.Reset(); normalRTV.Reset(); normalSRV.Reset();
        depthTex.Reset(); dsv.Reset(); depthSRV.Reset();

        // カラー
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = width; td.Height = height; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&td, nullptr, texture.GetAddressOf())))              return false;
        if (FAILED(device->CreateRenderTargetView(texture.Get(), nullptr, rtv.GetAddressOf())))   return false;
        if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf()))) return false;

        if (!forScene) return true;   // 色だけの描画先はここまで

        // 法線（カラーと同じ形式・同じ大きさ）
        if (FAILED(device->CreateTexture2D(&td, nullptr, normalTex.GetAddressOf())))                        return false;
        if (FAILED(device->CreateRenderTargetView(normalTex.Get(), nullptr, normalRTV.GetAddressOf())))   return false;
        if (FAILED(device->CreateShaderResourceView(normalTex.Get(), nullptr, normalSRV.GetAddressOf()))) return false;

        // 深度。「深度として書く」と「テクスチャとして読む」で形式の呼び名が違うので、
        // テクスチャ本体は型なし（TYPELESS）で作り、ビューごとに形式を指定する。
        D3D11_TEXTURE2D_DESC dd = {};
        dd.Width = width; dd.Height = height; dd.MipLevels = 1; dd.ArraySize = 1;
        dd.Format = DXGI_FORMAT_R24G8_TYPELESS; dd.SampleDesc.Count = 1;
        dd.Usage = D3D11_USAGE_DEFAULT;
        dd.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&dd, nullptr, depthTex.GetAddressOf()))) return false;

        D3D11_DEPTH_STENCIL_VIEW_DESC dvd = {};
        dvd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; dvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        if (FAILED(device->CreateDepthStencilView(depthTex.Get(), &dvd, dsv.GetAddressOf()))) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
        svd.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        svd.Texture2D.MipLevels = 1;
        if (FAILED(device->CreateShaderResourceView(depthTex.Get(), &svd, depthSRV.GetAddressOf()))) return false;

        return true;
    }

    void RenderTexture::Resize(ID3D11Device* device, int width, int height)
    {
        if (width <= 0 || height <= 0) return;
        if (width == this->width && height == this->height) return;
        Create(device, width, height, forScene);
    }

    void RenderTexture::Begin(ID3D11DeviceContext* context, float r, float g, float b, float a)
    {
        const float color[4] = { r, g, b, a };
        context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());   // dsv が無ければ深度なし
        context->ClearRenderTargetView(rtv.Get(), color);
        if (normalRTV) {
            const float noGeometry[4] = { 0.5f, 0.5f, 0.5f, 0.0f };      // a = 0：何も描かれていない（空）
            context->ClearRenderTargetView(normalRTV.Get(), noGeometry);
        }
        if (dsv) context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        D3D11_VIEWPORT vp = {};
        vp.Width = (float)width; vp.Height = (float)height;
        vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);
    }

    void RenderTexture::BindSceneTargets(ID3D11DeviceContext* context)
    {
        if (!normalRTV) return;   // 色だけの描画先では何もしない
        ID3D11RenderTargetView* targets[2] = { rtv.Get(), normalRTV.Get() };
        context->OMSetRenderTargets(2, targets, dsv.Get());
    }

} // namespace uel