#include "Engine/Graphics/CubeRenderer.h"
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {

    namespace {
        struct CubeVertex { XMFLOAT3 position; XMFLOAT3 normal; };
        struct CubeCB { XMFLOAT4X4 world; XMFLOAT4X4 viewProj; XMFLOAT4 color; XMFLOAT4 lightDir; };

        const char* kHLSL = R"(
cbuffer CB : register(b0) {
    float4x4 world;
    float4x4 viewProj;
    float4   color;
    float4   lightDir;
};
struct VSIn { float3 pos : POSITION; float3 nrm : NORMAL; };
struct PSIn { float4 pos : SV_POSITION; float3 nrm : NORMAL; };
PSIn VSMain(VSIn i) {
    PSIn o;
    float4 wpos = mul(float4(i.pos, 1.0), world);
    o.pos = mul(wpos, viewProj);
    o.nrm = mul(i.nrm, (float3x3)world);
    return o;
}
float4 PSMain(PSIn i) : SV_TARGET {
    float3 n = normalize(i.nrm);
    float3 l = normalize(-lightDir.xyz);
    float d = saturate(dot(n, l));
    float3 c = color.rgb * (0.25 + 0.75 * d);
    return float4(c, 1.0);
}
)";
    }

    bool CubeRenderer::Initialize(ID3D11Device* device)
    {
        // シェーダ
        ComPtr<ID3DBlob> vsBlob, psBlob, err;
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "cube", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, vsBlob.GetAddressOf(), err.GetAddressOf()))) return false;
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "cube", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, psBlob.GetAddressOf(), err.GetAddressOf()))) return false;
        if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs.GetAddressOf()))) return false;
        if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps.GetAddressOf()))) return false;

        // 入力レイアウト
        D3D11_INPUT_ELEMENT_DESC il[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(device->CreateInputLayout(il, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), layout.GetAddressOf()))) return false;

        // 立方体（面ごとに法線、24頂点）
        const CubeVertex v[] = {
            {{ 0.5f,-0.5f,-0.5f},{ 1,0,0}},{{ 0.5f, 0.5f,-0.5f},{ 1,0,0}},{{ 0.5f, 0.5f, 0.5f},{ 1,0,0}},{{ 0.5f,-0.5f, 0.5f},{ 1,0,0}},
            {{-0.5f,-0.5f, 0.5f},{-1,0,0}},{{-0.5f, 0.5f, 0.5f},{-1,0,0}},{{-0.5f, 0.5f,-0.5f},{-1,0,0}},{{-0.5f,-0.5f,-0.5f},{-1,0,0}},
            {{-0.5f, 0.5f,-0.5f},{0, 1,0}},{{-0.5f, 0.5f, 0.5f},{0, 1,0}},{{ 0.5f, 0.5f, 0.5f},{0, 1,0}},{{ 0.5f, 0.5f,-0.5f},{0, 1,0}},
            {{-0.5f,-0.5f, 0.5f},{0,-1,0}},{{-0.5f,-0.5f,-0.5f},{0,-1,0}},{{ 0.5f,-0.5f,-0.5f},{0,-1,0}},{{ 0.5f,-0.5f, 0.5f},{0,-1,0}},
            {{ 0.5f,-0.5f, 0.5f},{0,0, 1}},{{ 0.5f, 0.5f, 0.5f},{0,0, 1}},{{-0.5f, 0.5f, 0.5f},{0,0, 1}},{{-0.5f,-0.5f, 0.5f},{0,0, 1}},
            {{-0.5f,-0.5f,-0.5f},{0,0,-1}},{{-0.5f, 0.5f,-0.5f},{0,0,-1}},{{ 0.5f, 0.5f,-0.5f},{0,0,-1}},{{ 0.5f,-0.5f,-0.5f},{0,0,-1}},
        };
        UINT idx[36];
        for (int f = 0; f < 6; ++f) {
            int b = f * 4, o = f * 6;
            idx[o + 0] = b + 0; idx[o + 1] = b + 1; idx[o + 2] = b + 2;
            idx[o + 3] = b + 0; idx[o + 4] = b + 2; idx[o + 5] = b + 3;
        }
        indexCount = 36;

        D3D11_BUFFER_DESC bd = {}; D3D11_SUBRESOURCE_DATA sd = {};
        bd.Usage = D3D11_USAGE_IMMUTABLE; bd.ByteWidth = sizeof(v); bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; sd.pSysMem = v;
        if (FAILED(device->CreateBuffer(&bd, &sd, vb.GetAddressOf()))) return false;
        bd.ByteWidth = sizeof(idx); bd.BindFlags = D3D11_BIND_INDEX_BUFFER; sd.pSysMem = idx;
        if (FAILED(device->CreateBuffer(&bd, &sd, ib.GetAddressOf()))) return false;

        // 定数バッファ
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT; cbd.ByteWidth = sizeof(CubeCB); cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&cbd, nullptr, cb.GetAddressOf()))) return false;

        // ラスタライザ（事故防止でカリング無し）
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&rd, raster.GetAddressOf()))) return false;

        return true;
    }

    void CubeRenderer::Draw(ID3D11DeviceContext* context,
        const XMFLOAT4X4& world, const XMFLOAT4X4& viewProj, const XMFLOAT4& color)
    {
        CubeCB data;
        XMStoreFloat4x4(&data.world, XMMatrixTranspose(XMLoadFloat4x4(&world)));    // 転置してアップロード
        XMStoreFloat4x4(&data.viewProj, XMMatrixTranspose(XMLoadFloat4x4(&viewProj)));
        data.color = color;
        data.lightDir = XMFLOAT4(-0.5f, -1.0f, 0.3f, 0.0f);
        context->UpdateSubresource(cb.Get(), 0, nullptr, &data, 0, 0);

        UINT stride = sizeof(CubeVertex), offset = 0;
        context->IASetInputLayout(layout.Get());
        context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.Get(), nullptr, 0);
        context->PSSetShader(ps.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
        context->RSSetState(raster.Get());
        context->DrawIndexed(indexCount, 0, 0);
    }

} // namespace uel