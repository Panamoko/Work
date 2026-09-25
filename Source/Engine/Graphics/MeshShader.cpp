#include "Engine/Graphics/MeshShader.h"
#include "Engine/Graphics/Mesh.h"
#include <d3dcompiler.h>
#include <cstring>
#include <cstdint>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        struct CB {
            XMFLOAT4X4 world;
            XMFLOAT4X4 viewProj;
            XMFLOAT4   color;
            XMFLOAT4   lightDir;
            XMFLOAT4   lightColor;
            XMFLOAT4   params;
        };

        const char* kHLSL = R"(
Texture2D    tex  : register(t0);
SamplerState samp : register(s0);
cbuffer CB : register(b0) {
    float4x4 world;
    float4x4 viewProj;
    float4   color;
    float4   lightDir;
    float4   lightColor;
    float4   params;        // x=ambient, y=intensity
};
struct VSIn { float3 pos:POSITION; float3 nrm:NORMAL; float4 col:COLOR; float2 uv:TEXCOORD; };
struct PSIn { float4 pos:SV_POSITION; float3 nrm:NORMAL; float4 col:COLOR; float2 uv:TEXCOORD; };
PSIn VSMain(VSIn i) {
    PSIn o;
    float4 wpos = mul(float4(i.pos,1.0), world);
    o.pos = mul(wpos, viewProj);
    o.nrm = mul(i.nrm, (float3x3)world);
    o.col = i.col;
    o.uv  = i.uv;
    return o;
}
float4 PSMain(PSIn i) : SV_TARGET {
    float3 n = normalize(i.nrm);
    float3 l = normalize(-lightDir.xyz);
    float  d = saturate(dot(n, l));
    float4 t = tex.Sample(samp, i.uv);
    float3 base = t.rgb * i.col.rgb * color.rgb;
    float3 c = base * (params.x + d * params.y) * lightColor.rgb;   // ambient + diffuse
    return float4(c, 1.0);
}
)";
    }

    bool MeshShader::Initialize(ID3D11Device* device)
    {
        ComPtr<ID3DBlob> vsBlob, psBlob, err;
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "mesh", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, vsBlob.GetAddressOf(), err.GetAddressOf()))) return false;
        if (FAILED(D3DCompile(kHLSL, strlen(kHLSL), "mesh", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, psBlob.GetAddressOf(), err.GetAddressOf()))) return false;
        if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs.GetAddressOf()))) return false;
        if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps.GetAddressOf()))) return false;

        D3D11_INPUT_ELEMENT_DESC il[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(device->CreateInputLayout(il, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), layout.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT; cbd.ByteWidth = sizeof(CB); cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&cbd, nullptr, cb.GetAddressOf()))) return false;

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&rd, raster.GetAddressOf()))) return false;

        // サンプラ
        D3D11_SAMPLER_DESC smp = {};
        smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        smp.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&smp, sampler.GetAddressOf()))) return false;

        // 1x1 白テクスチャ（テクスチャ無しメッシュ用）
        uint32_t white = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC wd = {};
        wd.Width = wd.Height = 1; wd.MipLevels = 1; wd.ArraySize = 1;
        wd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; wd.SampleDesc.Count = 1;
        wd.Usage = D3D11_USAGE_IMMUTABLE; wd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA wsd = {}; wsd.pSysMem = &white; wsd.SysMemPitch = 4;
        ComPtr<ID3D11Texture2D> wtex;
        if (FAILED(device->CreateTexture2D(&wd, &wsd, wtex.GetAddressOf()))) return false;
        if (FAILED(device->CreateShaderResourceView(wtex.Get(), nullptr, whiteSRV.GetAddressOf()))) return false;

        return true;
    }

    void MeshShader::Draw(ID3D11DeviceContext* context, const Mesh& mesh,
        const XMFLOAT4X4& world, const XMFLOAT4X4& viewProj, const XMFLOAT4& color,
        const std::vector<uint8_t>* hiddenSubmesh,
        const std::vector<uint8_t>* submeshRole,
        bool firstPerson)
    {
        if (!mesh.IsValid()) return;

        CB data;
        XMStoreFloat4x4(&data.world, XMMatrixTranspose(XMLoadFloat4x4(&world)));
        XMStoreFloat4x4(&data.viewProj, XMMatrixTranspose(XMLoadFloat4x4(&viewProj)));
        data.color = color;
        data.lightDir = XMFLOAT4(lightDir.x, lightDir.y, lightDir.z, 0.0f);
        data.lightColor = XMFLOAT4(lightColor.x, lightColor.y, lightColor.z, 0.0f);
        data.params = XMFLOAT4(lightAmbient, lightIntensity, 0.0f, 0.0f);

        context->UpdateSubresource(cb.Get(), 0, nullptr, &data, 0, 0);   
        context->IASetInputLayout(layout.Get());
        context->VSSetShader(vs.Get(), nullptr, 0);
        context->PSSetShader(ps.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
        context->RSSetState(raster.Get());
        context->PSSetSamplers(0, 1, sampler.GetAddressOf());

        mesh.Bind(context);
        const auto& subs = mesh.GetSubmeshes();
        for (size_t i = 0; i < subs.size(); ++i) {
            const auto& sm = subs[i];
            if (!sm.visible || sm.indexCount == 0) continue;

            bool hide = false;
            if (hiddenSubmesh && i < hiddenSubmesh->size() && (*hiddenSubmesh)[i]) hide = true;          // 手動非表示
            if (firstPerson && submeshRole && i < submeshRole->size()
                && (*submeshRole)[i] == (uint8_t)1 /*FirstPersonHidden*/) hide = true;                   // 役割で非表示
            if (hide) continue;

            ID3D11ShaderResourceView* srv = sm.texture ? sm.texture.Get() : whiteSRV.Get();
            context->PSSetShaderResources(0, 1, &srv);
            context->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
        }
    }

    void MeshShader::SetLight(const SceneLighting& light)
    {
        lightDir = light.direction;
        lightColor = light.color;
        lightAmbient = light.ambient;
        lightIntensity = light.intensity;
    }

} // namespace uel