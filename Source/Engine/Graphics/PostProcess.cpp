#include "Engine/Graphics/PostProcess.h"
#include "Engine/Graphics/RenderTexture.h"
#include "Engine/Graphics/ShaderCommon.h"   // CompileShaderSource
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace uel {
    namespace {
        // HLSL 側の cbuffer PostCB と並びを一致させること。
        struct PostCB {
            XMFLOAT4 grade;        // x=saturation, y=debugView(PostDebugView)
            XMFLOAT4 depth;        // x=proj._33, y=proj._43（深度を距離に戻す係数）, z=1/幅, w=1/高さ
            XMFLOAT4 edgeColor;    // rgb=輪郭線の色, a=有効なら 1
            XMFLOAT4 edgeParams;   // x=太さ(px), y=深度のしきい値, z=法線のしきい値, w=線を出す最大距離
        };

        // 注意：HLSL の文字列の中には日本語コメントを書かない（ShaderCommon.h の冒頭を参照）。
        const char* kPostHLSL = R"(
cbuffer PostCB : register(b0) {
    float4 grade;        // x=saturation, y=debug view (0=Final, 1=Normal, 2=Depth, 3=Edge)
    float4 depth;        // x=proj._33, y=proj._43, z=1/width, w=1/height
    float4 edgeColor;    // rgb=outline color, a=1 when enabled
    float4 edgeParams;   // x=thickness (pixels), y=depth threshold, z=normal threshold, w=max distance
};
Texture2D    sceneTex  : register(t0);   // color
Texture2D    normalTex : register(t1);   // rgb = world normal * 0.5 + 0.5, a = 1 where something was drawn
Texture2D    depthTex  : register(t2);   // depth buffer value, 0 (near) .. 1 (far)
SamplerState samp      : register(s0);

struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };

// One big triangle that covers the whole screen. No vertex buffer: positions come from the vertex id.
//   id 0 -> uv (0,0) top-left,  id 1 -> uv (2,0),  id 2 -> uv (0,2)
VSOut VSMain(uint id : SV_VertexID) {
    VSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.uv  = uv;
    return o;
}

float RawDepth(float2 uv) { return depthTex.SampleLevel(samp, uv, 0).r; }

// Depth buffer value -> distance from the camera along its forward axis (world units).
// The depth buffer is not linear: most of its range is used up close. This undoes the projection.
float ToDistance(float rawDepth) { return depth.y / (rawDepth - depth.x); }

// 0 below the threshold, rising to 1 at 1.5 x threshold (a short ramp instead of a hard step).
float OverThreshold(float value, float threshold) {
    return saturate((value - threshold) / max(threshold * 0.5, 1e-5));
}

// ---- Edge detection ----
// Returns 0 (no line) .. 1 (line) for one pixel, by comparing it with its neighbours in 4 directions.
//
// Depth: uses the "second difference" of the raw depth value: a + b - 2 * center,
//   where a and b are the neighbours on opposite sides.
//   The raw depth value changes at a constant rate across any flat surface, however steeply it is viewed,
//   so this is exactly 0 on a floor or a wall and only becomes large where the depth jumps.
//   (Comparing distances directly would draw false lines all over a floor seen at a shallow angle.)
// Normal: 1 - dot(center, neighbour). 0 when they face the same way, 1 at a right angle.
//   This finds creases where the depth is continuous, e.g. where a wall meets the floor.
float EdgeAmount(float2 uv) {
    float2 pixel = depth.zw * max(edgeParams.x, 0.5);

    float  d0 = RawDepth(uv);
    float  z  = ToDistance(d0);
    float4 n0 = normalTex.SampleLevel(samp, uv, 0);
    float3 nc = n0.rgb * 2.0 - 1.0;

    static const float2 kDirs[4] = { float2(1.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0), float2(1.0, -1.0) };
    float depthEdge  = 0.0;
    float normalEdge = 0.0;
    [unroll] for (int k = 0; k < 4; ++k) {
        float2 o = kDirs[k] * pixel;

        depthEdge = max(depthEdge, abs(RawDepth(uv + o) + RawDepth(uv - o) - 2.0 * d0));

        float4 na = normalTex.SampleLevel(samp, uv + o, 0);
        float4 nb = normalTex.SampleLevel(samp, uv - o, 0);
        // a = 0 means sky (no normal there): the depth test handles those borders.
        if (n0.a > 0.5 && na.a > 0.5) normalEdge = max(normalEdge, 1.0 - dot(nc, na.rgb * 2.0 - 1.0));
        if (n0.a > 0.5 && nb.a > 0.5) normalEdge = max(normalEdge, 1.0 - dot(nc, nb.rgb * 2.0 - 1.0));
    }

    // Convert the raw depth difference into "size of the depth jump relative to the distance",
    // so one threshold works both near and far.
    float relativeJump = depthEdge * z / abs(depth.y);

    float e = max(OverThreshold(relativeJump, edgeParams.y), OverThreshold(normalEdge, edgeParams.z));

    // Far away, lines get too dense and flicker: fade them out before the max distance.
    e *= 1.0 - smoothstep(edgeParams.w * 0.7, edgeParams.w, z);
    return e;
}

float4 PSMain(VSOut i) : SV_TARGET {
    int view = (int)(grade.y + 0.5);

    // Debug views: show the buffers that the effects read.
    if (view == 1) return float4(normalTex.SampleLevel(samp, i.uv, 0).rgb, 1.0);
    if (view == 2) {
        float g = saturate(ToDistance(RawDepth(i.uv)) / 50.0);   // black at the camera, white at 50 m and beyond
        return float4(g, g, g, 1.0);
    }
    if (view == 3) {
        float g = 1.0 - EdgeAmount(i.uv);                        // detected lines only: black on white
        return float4(g, g, g, 1.0);
    }

    float3 c = sceneTex.SampleLevel(samp, i.uv, 0).rgb;

    // Saturation: blend between the gray version and the original color.
    float gray = dot(c, float3(0.299, 0.587, 0.114));
    c = lerp(float3(gray, gray, gray), c, grade.x);

    // Outline: drawn last so that its color is not changed by the grading above.
    if (edgeColor.a > 0.5) c = lerp(c, edgeColor.rgb, EdgeAmount(i.uv));

    return float4(saturate(c), 1.0);
}
)";
    }

    bool PostProcess::Initialize(ID3D11Device* device)
    {
        const std::string source = kPostHLSL;
        ComPtr<ID3DBlob> vsB, psB;
        if (!CompileShaderSource(source, "post", "VSMain", "vs_5_0", vsB.GetAddressOf())) return false;
        if (!CompileShaderSource(source, "post", "PSMain", "ps_5_0", psB.GetAddressOf())) return false;
        if (FAILED(device->CreateVertexShader(vsB->GetBufferPointer(), vsB->GetBufferSize(), nullptr, vs.GetAddressOf()))) return false;
        if (FAILED(device->CreatePixelShader(psB->GetBufferPointer(), psB->GetBufferSize(), nullptr, ps.GetAddressOf()))) return false;

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