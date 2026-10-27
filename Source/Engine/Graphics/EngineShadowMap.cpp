#include "Engine/Graphics/EngineShadowMap.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/SkinnedRenderModel.h"
#include "Engine/Graphics/ShaderLoader.h"
#include <vector>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        // Shader/Shadow.hlsli の cbuffer ShadowCB、Shader/Skinning.hlsli の cbuffer Bones と、並び・型を合わせる。
        constexpr int kMaxBones = 128;
        struct SCB { XMFLOAT4X4 lightViewProj; XMFLOAT4X4 world; };
        struct BoneCB { XMFLOAT4X4 bones[kMaxBones]; };
    }

    bool EngineShadowMap::Initialize(ID3D11Device* device, int sz)
    {
        size = sz;
        // 深度テクスチャ（DSV＋SRV）
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = td.Height = size; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32_TYPELESS; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&td, nullptr, tex.GetAddressOf()))) return false;

        D3D11_DEPTH_STENCIL_VIEW_DESC dvd = {};
        dvd.Format = DXGI_FORMAT_D32_FLOAT; dvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        if (FAILED(device->CreateDepthStencilView(tex.Get(), &dvd, dsv.GetAddressOf()))) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
        svd.Format = DXGI_FORMAT_R32_FLOAT; svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; svd.Texture2D.MipLevels = 1;
        if (FAILED(device->CreateShaderResourceView(tex.Get(), &svd, srv.GetAddressOf()))) return false;

        // 深度専用VS（静的メッシュ）。Shader/ShadowVS.hlsl（ビルド時に Data/Shader/ShadowVS.cso へコンパイルされる）
        std::vector<char> vsCode;   // 入力レイアウトを作るのに、頂点シェーダのバイトコードが要る
        if (!CreateVertexShaderFromCso(device, "ShadowVS", vs, &vsCode)) return false;

        // POSITIONのみの入力レイアウト（頂点の先頭にあるposだけ読む）
        D3D11_INPUT_ELEMENT_DESC il[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(device->CreateInputLayout(il, 1, vsCode.data(), vsCode.size(), layout.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC cbd = {}; cbd.Usage = D3D11_USAGE_DEFAULT; cbd.ByteWidth = sizeof(SCB); cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&cbd, nullptr, cb.GetAddressOf()))) return false;

        // 深度専用VS（スキンドメッシュ）。Shader/ShadowSkinnedVS.hlsl
        std::vector<char> svsCode;
        if (!CreateVertexShaderFromCso(device, "ShadowSkinnedVS", skinnedVS, &svsCode)) return false;

        // SkinnedVertex（80バイト）のうち、位置・ボーン番号・ウェイトだけ読む
        D3D11_INPUT_ELEMENT_DESC sil[] = {
            { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(device->CreateInputLayout(sil, 3, svsCode.data(), svsCode.size(), skinnedLayout.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC bbd = {}; bbd.Usage = D3D11_USAGE_DEFAULT; bbd.ByteWidth = sizeof(BoneCB); bbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&bbd, nullptr, boneCB.GetAddressOf()))) return false;
        return true;
    }

    void EngineShadowMap::BeginPass(ID3D11DeviceContext* ctx, const XMFLOAT4X4& lightViewProj)
    {
        lightVP = lightViewProj;
        ID3D11RenderTargetView* noRTV = nullptr;
        ctx->OMSetRenderTargets(0, &noRTV, dsv.Get());
        ctx->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        D3D11_VIEWPORT vp = {}; vp.Width = (float)size; vp.Height = (float)size; vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);
        ctx->PSSetShader(nullptr, nullptr, 0);   // 深度のみ
    }

    void EngineShadowMap::DrawMesh(ID3D11DeviceContext* ctx, const Mesh& mesh, const XMFLOAT4X4& world)
    {
        if (!mesh.IsValid()) return;
        SCB d;
        XMStoreFloat4x4(&d.lightViewProj, XMMatrixTranspose(XMLoadFloat4x4(&lightVP)));
        XMStoreFloat4x4(&d.world, XMMatrixTranspose(XMLoadFloat4x4(&world)));
        ctx->UpdateSubresource(cb.Get(), 0, nullptr, &d, 0, 0);

        ctx->IASetInputLayout(layout.Get());          // 静的とスキンドが混ざるので毎回設定する
        ctx->VSSetShader(vs.Get(), nullptr, 0);
        ctx->VSSetConstantBuffers(0, 1, cb.GetAddressOf());

        mesh.Bind(ctx);
        for (const auto& sm : mesh.GetSubmeshes()) {
            if (sm.indexCount == 0) continue;
            ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
        }
    }

    void EngineShadowMap::DrawSkinned(ID3D11DeviceContext* ctx, const SkinnedRenderModel& model,
        const XMFLOAT4X4& world, const std::vector<XMFLOAT4X4>& palette)
    {
        if (!model.Valid()) return;
        SCB d;
        XMStoreFloat4x4(&d.lightViewProj, XMMatrixTranspose(XMLoadFloat4x4(&lightVP)));
        XMStoreFloat4x4(&d.world, XMMatrixTranspose(XMLoadFloat4x4(&world)));
        ctx->UpdateSubresource(cb.Get(), 0, nullptr, &d, 0, 0);

        BoneCB b;
        int n = (int)palette.size(); if (n > kMaxBones) n = kMaxBones;
        for (int i = 0; i < n; ++i) XMStoreFloat4x4(&b.bones[i], XMMatrixTranspose(XMLoadFloat4x4(&palette[i])));
        for (int i = n; i < kMaxBones; ++i) XMStoreFloat4x4(&b.bones[i], XMMatrixIdentity());
        ctx->UpdateSubresource(boneCB.Get(), 0, nullptr, &b, 0, 0);

        ctx->IASetInputLayout(skinnedLayout.Get());
        ctx->VSSetShader(skinnedVS.Get(), nullptr, 0);
        ID3D11Buffer* cbs[2] = { cb.Get(), boneCB.Get() };
        ctx->VSSetConstantBuffers(0, 2, cbs);

        model.Bind(ctx);
        for (const auto& sm : model.Submeshes()) {
            if (sm.indexCount == 0) continue;
            ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
        }
    }
}