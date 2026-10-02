#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <string>
#include "Engine/Graphics/ShadingParams.h"

// 静的メッシュ（MeshShader）とスキンドメッシュ（SkinnedMeshShader）で共通に使う部分。
//   ・b0 の定数バッファの形（C++ 側 / HLSL 側）
//   ・影の計算とピクセルシェーダ（ライティングの式）
//   ・HLSL のコンパイル（失敗したらエラー内容を「出力」ウィンドウに出す）
// 見た目の式を変えるときは、このファイルだけ直せば両方に効く。
//
// 注意：HLSL の文字列の中には日本語コメントを書かない。
//       ソースは Shift-JIS なので、文字によっては 2 バイト目が '\' になり、
//       行末にあると「次の行もコメント」と解釈されてしまう。

namespace uel {

    // b0：オブジェクト＋ライト。
    // 下の kShaderCommonHLSL の「cbuffer ObjectCB」と、並び・型を必ず一致させること。
    struct ObjectCB {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 viewProj;
        DirectX::XMFLOAT4X4 lightViewProj;
        DirectX::XMFLOAT4   color;
        DirectX::XMFLOAT4   lightDir;
        DirectX::XMFLOAT4   lightColor;
        DirectX::XMFLOAT4   params;          // x=ambient, y=intensity, z=shadowEnabled
        DirectX::XMFLOAT4   shading;         // x=mode(ShadingMode), y=receiveShadow
    };

    // ShadingParams（描き方のデータ）を定数バッファに詰める。
    // 項目を増やすときは、ここ・ObjectCB・HLSL の3か所をそろえる。
    inline void FillShading(ObjectCB& cb, const ShadingParams& s)
    {
        cb.shading = DirectX::XMFLOAT4(
            static_cast<float>(static_cast<int>(s.mode)),
            s.receiveShadow ? 1.0f : 0.0f,
            0.0f, 0.0f);
    }

    // 共通の HLSL。各シェーダは「これ ＋ 自分の頂点シェーダ（VSIn / VSMain）」を連結してコンパイルする。
    // 頂点シェーダ側は PSIn を埋めて返すこと。
    inline const char* const kShaderCommonHLSL = R"(
cbuffer ObjectCB : register(b0) {
    float4x4 world;
    float4x4 viewProj;
    float4x4 lightViewProj;
    float4   color;
    float4   lightDir;
    float4   lightColor;
    float4   params;      // x=ambient, y=intensity, z=shadowEnabled
    float4   shading;     // x=mode (0=Lit, 1=Unlit), y=receiveShadow
};
Texture2D    tex        : register(t0);
Texture2D    shadowMap  : register(t1);
SamplerState samp       : register(s0);
SamplerComparisonState shadowSamp : register(s1);

struct PSIn { float4 pos:SV_POSITION; float3 nrm:NORMAL; float4 col:COLOR; float2 uv:TEXCOORD; float3 wpos:WPOS; };

float ShadowFactor(float3 wpos) {
    float4 lp = mul(float4(wpos, 1.0), lightViewProj);
    lp.xyz /= lp.w;
    float2 uv = lp.xy * float2(0.5, -0.5) + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 1.0;
    float depth = lp.z - 0.0025;
    float texel = 1.0 / 2048.0;
    float sum = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x)
        sum += shadowMap.SampleCmpLevelZero(shadowSamp, uv + float2(x, y) * texel, depth);
    return sum / 9.0;
}

float4 PSMain(PSIn i) : SV_TARGET {
    float4 t = tex.Sample(samp, i.uv);
    float3 base = t.rgb * i.col.rgb * color.rgb;

    // mode must match ShadingMode in ShadingParams.h
    int mode = (int)(shading.x + 0.5);
    if (mode == 1) return float4(base, 1.0);   // Unlit: texture * color only

    // Lit: ambient + directional light * shadow
    float3 n = normalize(i.nrm);
    float3 l = normalize(-lightDir.xyz);
    float  d = saturate(dot(n, l));
    float  sh = (params.z > 0.5 && shading.y > 0.5) ? ShadowFactor(i.wpos) : 1.0;
    float3 c = base * (params.x + d * params.y * sh) * lightColor.rgb;
    return float4(c, 1.0);
}
)";

    // HLSL をコンパイルする。失敗したら、エラー内容を Visual Studio の「出力」ウィンドウに出して false を返す。
    //   name   : エラー表示に使う名前（"mesh" など）
    //   entry  : 開始関数名（"VSMain" / "PSMain"）
    //   target : "vs_5_0" / "ps_5_0"
    inline bool CompileShaderSource(const std::string& source, const char* name,
        const char* entry, const char* target, ID3DBlob** outBlob)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> err;
        HRESULT hr = D3DCompile(source.data(), source.size(), name, nullptr, nullptr,
            entry, target, 0, 0, outBlob, err.GetAddressOf());
        if (FAILED(hr)) {
            OutputDebugStringA("[Shader] compile failed: ");
            OutputDebugStringA(name);
            OutputDebugStringA(" / ");
            OutputDebugStringA(entry);
            OutputDebugStringA("\n");
            if (err) {
                OutputDebugStringA(static_cast<const char*>(err->GetBufferPointer()));
                OutputDebugStringA("\n");
            }
            return false;
        }
        return true;
    }

} // namespace uel