#include "Engine/Graphics/PostProcess.h"
#include "Engine/Graphics/RenderTexture.h"
#include "Engine/Graphics/ShaderLoader.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        // Shader/PostProcess.hlsli の cbuffer PostCB と、並び・型を必ず一致させること。
        struct PostCB {
            XMFLOAT4 grade;        // x=saturation, y=debugView(PostDebugView)
            XMFLOAT4 depth;        // x=proj._33, y=proj._43（深度を距離に戻す係数）, z=1/幅, w=1/高さ
            XMFLOAT4 edgeColor;    // rgb=輪郭線の色, a=有効なら 1
            XMFLOAT4 edgeParams;   // x=太さ(px), y=深度のしきい値, z=法線のしきい値, w=線を出す最大距離
        };
    }

    bool PostProcess::Initialize(ID3D11Device* device)
    {
        // シェーダは Shader/PostProcessVS.hlsl・PostProcessPS.hlsl。ビルド時に Data/Shader/*.cso へコンパイルされる。
        if (!CreateVertexShaderFromCso(device, "PostProcessVS", vs)) return false;
        if (!CreatePixelShaderFromCso(device, "PostProcessPS", ps)) return false;

        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT; bd.ByteWidth = sizeof(PostCB); bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&bd, nullptr, cb.GetAddressOf()))) return false;

        // 元の画像と同じ大きさで読むので、補間なし・端は引き伸ばし
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sd, sampler.GetAddressOf()))) return false;

        // 画面全体を上書きするだけなので、深度は使わない
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable = FALSE; dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        if (FAILED(device->CreateDepthStencilState(&dsd, noDepth.GetAddressOf()))) return false;

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&rd, raster.GetAddressOf()))) return false;

        return true;
    }

    void PostProcess::Apply(ID3D11DeviceContext* context, const RenderTexture& scene, RenderTexture& dest,
        const PostProcessSettings& settings, const XMFLOAT4X4& proj)
    {
        // 描画先を dest に切り替える（これで scene の色・法線・深度は描画先から外れ、読めるようになる）
        dest.Begin(context, 0.0f, 0.0f, 0.0f, 1.0f);
        if (!scene.GetSRV() || !vs || !ps) return;

        const float invW = (dest.GetWidth() > 0) ? 1.0f / (float)dest.GetWidth() : 0.0f;
        const float invH = (dest.GetHeight() > 0) ? 1.0f / (float)dest.GetHeight() : 0.0f;

        PostCB data;
        data.grade = XMFLOAT4(settings.saturation, static_cast<float>(static_cast<int>(settings.debugView)), 0.0f, 0.0f);
        data.depth = XMFLOAT4(proj._33, proj._43, invW, invH);
        data.edgeColor = XMFLOAT4(settings.edgeColor[0], settings.edgeColor[1], settings.edgeColor[2],
            settings.edgeEnabled ? 1.0f : 0.0f);
        data.edgeParams = XMFLOAT4(settings.edgeThickness, settings.edgeDepthThreshold,
            settings.edgeNormalThreshold, settings.edgeFadeDistance);
        context->UpdateSubresource(cb.Get(), 0, nullptr, &data, 0, 0);

        context->OMSetDepthStencilState(noDepth.Get(), 0);
        context->RSSetState(raster.Get());
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.Get(), nullptr, 0);
        context->PSSetShader(ps.Get(), nullptr, 0);
        context->PSSetConstantBuffers(0, 1, cb.GetAddressOf());
        ID3D11ShaderResourceView* inputs[3] = { scene.GetSRV(), scene.GetNormalSRV(), scene.GetDepthSRV() };   // t0, t1, t2
        context->PSSetShaderResources(0, 3, inputs);
        context->PSSetSamplers(0, 1, sampler.GetAddressOf());
        context->Draw(3, 0);

        // 読んでいた3枚を外す（次のフレームで、また描画先として使うため）
        ID3D11ShaderResourceView* none[3] = { nullptr, nullptr, nullptr };
        context->PSSetShaderResources(0, 3, none);
        context->OMSetDepthStencilState(nullptr, 0);   // 既定（深度有効）へ戻す
    }

} // namespace uel
