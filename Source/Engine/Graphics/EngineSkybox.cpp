#include "Engine/Graphics/EngineSkybox.h"
#include <d3dcompiler.h>
#include <cstring>
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        struct CB { XMFLOAT4X4 invViewProj; XMFLOAT4 camPos; };

        const char* kHLSL = R"(
cbuffer CB : register(b0) { float4x4 invViewProj; float4 camPos; };
struct VSOut { float4 pos : SV_POSITION; float2 ndc : TEXCOORD; };
VSOut VSMain(uint id : SV_VertexID) {
    VSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);          // 0,0 / 2,0 / 0,2
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0); // 全画面三角形
    o.ndc = o.pos.xy;
    return o;
}
float4 PSMain(VSOut i) : SV_TARGET {
    float4 world = mul(float4(i.ndc, 1.0, 1.0), invViewProj);
    float3 wp  = world.xyz / world.w;
    float3 dir = normalize(wp - camPos.xyz);
    float3 horizon = float3(0.55, 0.65, 0.78);
    float3 zenith  = float3(0.13, 0.28, 0.55);
    float3 groundc = float3(0.18, 0.20, 0.23);
    float3 sky = (dir.y >= 0.0) ? lerp(horizon, zenith, saturate(dir.y))
                                : lerp(horizon, groundc, saturate(-dir.y));
    return float4(sky, 1.0);
}
)";
    }

    bool EngineSkybox::Initialize(ID3D11Device* device)
    {
        ComPtr<ID3DBlob> vsB, psB, err;
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "sky", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, vsB.GetAddressOf(), err.GetAddressOf()))) return false;
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "sky", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, psB.GetAddressOf(), err.GetAddressOf()))) return false;
        if (FAILED(device->CreateVertexShader(vsB->GetBufferPointer(), vsB->GetBufferSize(), nullptr, vs.GetAddressOf()))) return false;
        if (FAILED(device->CreatePixelShader(psB->GetBufferPointer(), psB->GetBufferSize(), nullptr, ps.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC bd = {}; bd.Usage = D3D11_USAGE_DEFAULT; bd.ByteWidth = sizeof(CB); bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&bd, nullptr, cb.GetAddressOf()))) return false;

        D3D11_DEPTH_STENCIL_DESC dsd = {}; dsd.DepthEnable = FALSE; dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        if (FAILED(device->CreateDepthStencilState(&dsd, noDepth.GetAddressOf()))) return false;

        D3D11_RASTERIZER_DESC rd = {}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&rd, raster.GetAddressOf()))) return false;
        return true;
    }

    void EngineSkybox::Render(ID3D11DeviceContext* ctx, const XMFLOAT4X4& viewProj, const XMFLOAT3& camPos)
    {
        CB data;
        XMMATRIX vp = XMLoadFloat4x4(&viewProj);
        XMMATRIX inv = XMMatrixInverse(nullptr, vp);
        XMStoreFloat4x4(&data.invViewProj, XMMatrixTranspose(inv));
        data.camPos = XMFLOAT4(camPos.x, camPos.y, camPos.z, 1.0f);
        ctx->UpdateSubresource(cb.Get(), 0, nullptr, &data, 0, 0);

        ctx->OMSetDepthStencilState(noDepth.Get(), 0);
        ctx->RSSetState(raster.Get());
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs.Get(), nullptr, 0);
        ctx->PSSetShader(ps.Get(), nullptr, 0);
        ctx->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
        ctx->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
        ctx->Draw(3, 0);

        ctx->OMSetDepthStencilState(nullptr, 0); // 既定（深度有効）へ戻す
    }
}