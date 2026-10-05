#include "Engine/Graphics/SkinnedMeshShader.h"
#include "Engine/Graphics/SkinnedRenderModel.h"
#include "Engine/Graphics/ShaderCommon.h"
#include <string>
#include <cstdint>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        constexpr int kMaxBones = 128;
        struct BoneCB { XMFLOAT4X4 bones[kMaxBones]; };

        // スキンドメッシュ専用の部分（ボーンの定数バッファと頂点シェーダだけ）。
        // b0 の定数バッファ・影・ピクセルシェーダは ShaderCommon.h の kShaderCommonHLSL にある。
        // ToWorld でスキニング後のワールド位置と法線を求め、通常用とアウトライン用の両方で使う。
        const char* kSkinnedVS = R"(
cbuffer Bones : register(b1) { float4x4 bones[128]; };
struct VSIn {
    float3 pos:POSITION; float3 nrm:NORMAL; float4 col:COLOR; float2 uv:TEXCOORD;
    uint4 bidx:BLENDINDICES; float4 bw:BLENDWEIGHT;
};
void ToWorld(VSIn i, out float3 wpos, out float3 wnrm) {
    float4 p = float4(i.pos, 1.0);
    float3 sp = 0; float3 sn = 0; float wsum = 0;
    [unroll] for (int k = 0; k < 4; ++k) {
        float w = i.bw[k]; uint b = i.bidx[k];
        sp += w * mul(p, bones[b]).xyz;
        sn += w * mul(i.nrm, (float3x3)bones[b]);
        wsum += w;
    }
    if (wsum < 1e-4) { sp = i.pos; sn = i.nrm; }
    wpos = mul(float4(sp, 1.0), world).xyz;
    wnrm = mul(sn, (float3x3)world);
}
PSIn VSMain(VSIn i)    { float3 p; float3 n; ToWorld(i, p, n); return MakePSIn(p, n, i.col, i.uv); }
PSIn VSOutline(VSIn i) { float3 p; float3 n; ToWorld(i, p, n); return MakeOutlinePSIn(p, n, i.col, i.uv); }
)";
    }

    bool SkinnedMeshShader::Initialize(ID3D11Device* device)
    {
        // 共通部分 ＋ 自分の頂点シェーダ、の順に連結する（PSIn が先に宣言されている必要がある）
        const std::string source = std::string(kShaderCommonHLSL) + kSkinnedVS;

        ComPtr<ID3DBlob> vsB, psB;
        if (!CompileShaderSource(source, "skinned", "VSMain", "vs_5_0", vsB.GetAddressOf())) return false;
        if (!CompileShaderSource(source, "skinned", "PSMain", "ps_5_0", psB.GetAddressOf())) return false;
        if (FAILED(device->CreateVertexShader(vsB->GetBufferPointer(), vsB->GetBufferSize(), nullptr, vs.GetAddressOf()))) return false;
        if (FAILED(device->CreatePixelShader(psB->GetBufferPointer(), psB->GetBufferSize(), nullptr, ps.GetAddressOf()))) return false;

        // アウトライン用（頂点の形は同じなので、入力レイアウトは下の1つを共用する）
        ComPtr<ID3DBlob> vsOutlineB, psOutlineB;
        if (!CompileShaderSource(source, "skinned", "VSOutline", "vs_5_0", vsOutlineB.GetAddressOf())) return false;
        if (!CompileShaderSource(source, "skinned", "PSOutline", "ps_5_0", psOutlineB.GetAddressOf())) return false;
        if (FAILED(device->CreateVertexShader(vsOutlineB->GetBufferPointer(), vsOutlineB->GetBufferSize(), nullptr, vsOutline.GetAddressOf()))) return false;
        if (FAILED(device->CreatePixelShader(psOutlineB->GetBufferPointer(), psOutlineB->GetBufferSize(), nullptr, psOutline.GetAddressOf()))) return false;

        D3D11_INPUT_ELEMENT_DESC il[] = {
            { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(device->CreateInputLayout(il, 6, vsB->GetBufferPointer(), vsB->GetBufferSize(), layout.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC od = {}; od.Usage = D3D11_USAGE_DEFAULT; od.ByteWidth = sizeof(ObjectCB); od.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&od, nullptr, objCB.GetAddressOf()))) return false;
        D3D11_BUFFER_DESC bd = {}; bd.Usage = D3D11_USAGE_DEFAULT; bd.ByteWidth = sizeof(BoneCB); bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&bd, nullptr, boneCB.GetAddressOf()))) return false;

        D3D11_RASTERIZER_DESC rd = {}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&rd, raster.GetAddressOf()))) return false;

        // アウトライン用：表面を捨てて裏面だけ描く（時計回りが表。静的メッシュ側と同じ設定）
        D3D11_RASTERIZER_DESC ord = {}; ord.FillMode = D3D11_FILL_SOLID; ord.CullMode = D3D11_CULL_FRONT; ord.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&ord, rasterOutline.GetAddressOf()))) return false;

        // 通常のサンプラ（s0）。静的メッシュ側と同じ設定。
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sd, sampler.GetAddressOf()))) return false;

        // 影用の比較サンプラ（s1）。静的メッシュ側と同じ設定。
        D3D11_SAMPLER_DESC ssd = {};
        ssd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        ssd.AddressU = ssd.AddressV = ssd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        ssd.BorderColor[0] = ssd.BorderColor[1] = ssd.BorderColor[2] = ssd.BorderColor[3] = 1.0f;
        ssd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        if (FAILED(device->CreateSamplerState(&ssd, shadowSampler.GetAddressOf()))) return false;

        // 1x1 の白テクスチャ（テクスチャを持たないサブメッシュ用）
        uint32_t white = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC wd = {}; wd.Width = wd.Height = 1; wd.MipLevels = 1; wd.ArraySize = 1;
        wd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; wd.SampleDesc.Count = 1; wd.Usage = D3D11_USAGE_IMMUTABLE; wd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA wsd = {}; wsd.pSysMem = &white; wsd.SysMemPitch = 4;
        ComPtr<ID3D11Texture2D> wtex;
        if (FAILED(device->CreateTexture2D(&wd, &wsd, wtex.GetAddressOf()))) return false;
        if (FAILED(device->CreateShaderResourceView(wtex.Get(), nullptr, whiteSRV.GetAddressOf()))) return false;
        return true;
    }

    void SkinnedMeshShader::Draw(ID3D11DeviceContext* ctx, const SkinnedRenderModel& model,
        const XMFLOAT4X4& world, const XMFLOAT4X4& viewProj, const XMFLOAT4& color,
        const ShadingParams& shading,
        const std::vector<XMFLOAT4X4>& palette)
    {
        if (!model.Valid()) return;

        ObjectCB o;
        XMStoreFloat4x4(&o.world, XMMatrixTranspose(XMLoadFloat4x4(&world)));
        XMStoreFloat4x4(&o.viewProj, XMMatrixTranspose(XMLoadFloat4x4(&viewProj)));
        XMStoreFloat4x4(&o.lightViewProj, XMMatrixTranspose(XMLoadFloat4x4(&lightViewProj)));
        o.color = color;
        o.lightDir = XMFLOAT4(lightDir.x, lightDir.y, lightDir.z, 0.0f);
        o.lightColor = XMFLOAT4(lightColor.x, lightColor.y, lightColor.z, 0.0f);
        o.params = XMFLOAT4(lightAmbient, lightIntensity, shadowEnabled ? 1.0f : 0.0f, 0.0f);
        FillShading(o, shading);
        o.cameraPos = XMFLOAT4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);
        ctx->UpdateSubresource(objCB.Get(), 0, nullptr, &o, 0, 0);

        BoneCB b;
        int n = (int)palette.size(); if (n > kMaxBones) n = kMaxBones;
        for (int i = 0; i < n; ++i) XMStoreFloat4x4(&b.bones[i], XMMatrixTranspose(XMLoadFloat4x4(&palette[i])));
        for (int i = n; i < kMaxBones; ++i) XMStoreFloat4x4(&b.bones[i], XMMatrixIdentity());
        ctx->UpdateSubresource(boneCB.Get(), 0, nullptr, &b, 0, 0);

        ctx->IASetInputLayout(layout.Get());
        ctx->VSSetShader(vs.Get(), nullptr, 0);
        ctx->PSSetShader(ps.Get(), nullptr, 0);
        ID3D11Buffer* cbs[2] = { objCB.Get(), boneCB.Get() };
        ctx->VSSetConstantBuffers(0, 2, cbs);
        ctx->PSSetConstantBuffers(0, 1, cbs);
        ctx->RSSetState(raster.Get());
        ctx->PSSetSamplers(0, 1, sampler.GetAddressOf());
        ctx->PSSetShaderResources(1, 1, &shadowSRV);
        ctx->PSSetSamplers(1, 1, shadowSampler.GetAddressOf());

        model.Bind(ctx);
        const auto& subs = model.Submeshes();

        // 本描画
        for (size_t i = 0; i < subs.size(); ++i) {
            if (subs[i].indexCount == 0) continue;
            ID3D11ShaderResourceView* srv = model.TexSRV(subs[i].texture);
            if (!srv) srv = whiteSRV.Get();
            ctx->PSSetShaderResources(0, 1, &srv);
            ctx->DrawIndexed(subs[i].indexCount, subs[i].indexOffset, 0);
        }

        // アウトライン（背面法）：法線方向に少しふくらませた形の「裏面だけ」を単色で描く。
        // 元の形より外にはみ出た部分だけが見えるので、輪郭線になる。
        // 定数バッファ（b0 / b1）は本描画と同じものを使うので、同じ姿勢のままふくらむ。
        if (shading.outlineWidth > 0.0f) {
            ctx->VSSetShader(vsOutline.Get(), nullptr, 0);
            ctx->PSSetShader(psOutline.Get(), nullptr, 0);
            ctx->RSSetState(rasterOutline.Get());
            for (size_t i = 0; i < subs.size(); ++i) {
                if (subs[i].indexCount == 0) continue;
                ctx->DrawIndexed(subs[i].indexCount, subs[i].indexOffset, 0);
            }
        }
    }

    void SkinnedMeshShader::SetLight(const SceneLighting& light)
    {
        lightDir = light.direction; lightColor = light.color;
        lightAmbient = light.ambient; lightIntensity = light.intensity;
    }

    void SkinnedMeshShader::SetShadow(ID3D11ShaderResourceView* srv, const XMFLOAT4X4& lvp)
    {
        shadowSRV = srv;
        lightViewProj = lvp;
        shadowEnabled = (srv != nullptr);
    }

} // namespace uel