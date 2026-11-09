#include "Engine/Graphics/EngineSkybox.h"
#include "Engine/Graphics/ShaderLoader.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        // Shader/Skybox.hlsli の cbuffer SkyCB と、並び・型を必ず一致させること。
        struct SkyCB { XMFLOAT4X4 invViewProj; XMFLOAT4 camPos; XMFLOAT4 skyParams; };
    }

    bool EngineSkybox::Initialize(ID3D11Device* device)
    {
        // シェーダは Shader/SkyboxVS.hlsl・SkyboxPS.hlsl。ビルド時に Data/Shader/*.cso へコンパイルされる。
        if (!CreateVertexShaderFromCso(device, "SkyboxVS", vs)) return false;
        if (!CreatePixelShaderFromCso(device, "SkyboxPS", ps)) return false;

        D3D11_BUFFER_DESC bd = {}; bd.Usage = D3D11_USAGE_DEFAULT; bd.ByteWidth = sizeof(SkyCB); bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&bd, nullptr, cb.GetAddressOf()))) return false;

        D3D11_DEPTH_STENCIL_DESC dsd = {}; dsd.DepthEnable = FALSE; dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        if (FAILED(device->CreateDepthStencilState(&dsd, noDepth.GetAddressOf()))) return false;

        D3D11_RASTERIZER_DESC rd = {}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&rd, raster.GetAddressOf()))) return false;

        // 空の画像用：線形補間（ミップの間も）、端で止める
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sd, sampler.GetAddressOf()))) return false;
        return true;
    }

    void EngineSkybox::Render(ID3D11DeviceContext* ctx, const XMFLOAT4X4& viewProj, const XMFLOAT3& camPos,
        ID3D11ShaderResourceView* skyCube, const SceneLighting& lighting)
    {
        if (!vs || !ps) return;   // シェーダが読めなかったときは描かない（背景はクリアした色のまま）

        SkyCB data;
        XMMATRIX vp = XMLoadFloat4x4(&viewProj);
        XMMATRIX inv = XMMatrixInverse(nullptr, vp);
        XMStoreFloat4x4(&data.invViewProj, XMMatrixTranspose(inv));
        data.camPos = XMFLOAT4(camPos.x, camPos.y, camPos.z, skyCube ? 1.0f : 0.0f);   // w：空の画像を使うか
        data.skyParams = SkyShaderParams(lighting);   // x：明るさ、y / z：回転の cos / sin
        ctx->UpdateSubresource(cb.Get(), 0, nullptr, &data, 0, 0);

        ctx->OMSetDepthStencilState(noDepth.Get(), 0);
        ctx->RSSetState(raster.Get());
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs.Get(), nullptr, 0);
        ctx->PSSetShader(ps.Get(), nullptr, 0);
        ctx->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
        ctx->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
        if (skyCube) {
            ctx->PSSetShaderResources(0, 1, &skyCube);
            ctx->PSSetSamplers(0, 1, sampler.GetAddressOf());
        }
        ctx->Draw(3, 0);

        ctx->OMSetDepthStencilState(nullptr, 0); // 既定（深度有効）へ戻す
    }
}