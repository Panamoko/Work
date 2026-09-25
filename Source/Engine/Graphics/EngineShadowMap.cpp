#include "Engine/Graphics/EngineShadowMap.h"
#include "Engine/Graphics/Mesh.h"
#include <d3dcompiler.h>
#include <cstring>
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        struct SCB { XMFLOAT4X4 lightViewProj; XMFLOAT4X4 world; };
        const char* kVS = R"(
cbuffer CB : register(b0) { float4x4 lightViewProj; float4x4 world; };
float4 VSMain(float3 pos : POSITION) : SV_POSITION {
    float4 wp = mul(float4(pos, 1.0), world);
    return mul(wp, lightViewProj);
}
)";
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

        // 深度専用VS
        ComPtr<ID3DBlob> vsB, err;
        if (FAILED(D3DCompile(kVS, strlen(kVS), "shadow", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, vsB.GetAddressOf(), err.GetAddressOf()))) return false;
        if (FAILED(device->CreateVertexShader(vsB->GetBufferPointer(), vsB->GetBufferSize(), nullptr, vs.GetAddressOf()))) return false;

        // POSITIONのみの入力レイアウト（頂点stride 48でも先頭のposを読む）
        D3D11_INPUT_ELEMENT_DESC il[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(device->CreateInputLayout(il, 1, vsB->GetBufferPointer(), vsB->GetBufferSize(), layout.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC cbd = {}; cbd.Usage = D3D11_USAGE_DEFAULT; cbd.ByteWidth = sizeof(SCB); cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&cbd, nullptr, cb.GetAddressOf()))) return false;
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
        ctx->IASetInputLayout(layout.Get());
        ctx->VSSetShader(vs.Get(), nullptr, 0);
        ctx->PSSetShader(nullptr, nullptr, 0);   // 深度のみ
    }

    void EngineShadowMap::DrawMesh(ID3D11DeviceContext* ctx, const Mesh& mesh, const XMFLOAT4X4& world)
    {
        if (!mesh.IsValid()) return;
        SCB d;
        XMStoreFloat4x4(&d.lightViewProj, XMMatrixTranspose(XMLoadFloat4x4(&lightVP)));
        XMStoreFloat4x4(&d.world, XMMatrixTranspose(XMLoadFloat4x4(&world)));
        ctx->UpdateSubresource(cb.Get(), 0, nullptr, &d, 0, 0);
        ctx->VSSetConstantBuffers(0, 1, cb.GetAddressOf());

        mesh.Bind(ctx);
        for (const auto& sm : mesh.GetSubmeshes()) {
            if (sm.indexCount == 0) continue;
            ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
        }
    }
}