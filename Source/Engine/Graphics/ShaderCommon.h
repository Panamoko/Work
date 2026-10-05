#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <string>
#include "Engine/Graphics/ShadingParams.h"

// 静的メッシュ（MeshShader）とスキンドメッシュ（SkinnedMeshShader）で共通に使う部分。
//   ・b0 の定数バッファの形（C++ 側 / HLSL 側）
//   ・頂点シェーダの出力を組み立てる関数（通常用 / アウトライン用）
//   ・影の計算とピクセルシェーダ（ライティングの式、アウトラインの色）
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
        DirectX::XMFLOAT4   cameraPos;       // xyz=カメラのワールド位置（各シェーダの Draw で入れる）
        DirectX::XMFLOAT4   shading;         // x=mode(ShadingMode), y=receiveShadow, z=metallic
        DirectX::XMFLOAT4   specRim;         // x=specularStrength, y=roughness, z=rimStrength, w=rimPower
        DirectX::XMFLOAT4   rimColor;        // rgb=リムライトの色
        DirectX::XMFLOAT4   toon;            // x=threshold, y=softness, z=texSteps, w=texSmooth
        DirectX::XMFLOAT4   toonShade;       // rgb=陰の面に掛ける色
        DirectX::XMFLOAT4   outline;         // rgb=アウトラインの色, a=太さ（ワールドの長さ）
    };

    // ShadingParams（描き方のデータ）を定数バッファに詰める。
    // 項目を増やすときは、ここ・ObjectCB・HLSL の3か所をそろえる。
    inline void FillShading(ObjectCB& cb, const ShadingParams& s)
    {
        cb.shading = DirectX::XMFLOAT4(
            static_cast<float>(static_cast<int>(s.mode)),
            s.receiveShadow ? 1.0f : 0.0f,
            s.metallic, 0.0f);
        cb.specRim = DirectX::XMFLOAT4(s.specularStrength, s.roughness, s.rimStrength, s.rimPower);
        cb.rimColor = DirectX::XMFLOAT4(s.rimColor.x, s.rimColor.y, s.rimColor.z, 0.0f);
        cb.toon = DirectX::XMFLOAT4(s.toonThreshold, s.toonSoftness, s.toonTexSteps, s.toonTexSmooth);
        cb.toonShade = DirectX::XMFLOAT4(s.toonShadeColor.x, s.toonShadeColor.y, s.toonShadeColor.z, 0.0f);
        cb.outline = DirectX::XMFLOAT4(s.outlineColor.x, s.outlineColor.y, s.outlineColor.z, s.outlineWidth);
    }

    // 共通の HLSL。各シェーダは「これ ＋ 自分の頂点シェーダ」を連結してコンパイルする。
    // 頂点シェーダ側は、ワールド位置とワールド法線を自分の方法で求めてから、
    //   VSMain    → MakePSIn(...)          （通常の描画）
    //   VSOutline → MakeOutlinePSIn(...)   （アウトライン用。法線方向に押し出す）
    // を呼んで返すこと。
    inline const char* const kShaderCommonHLSL = R"(
cbuffer ObjectCB : register(b0) {
    float4x4 world;
    float4x4 viewProj;
    float4x4 lightViewProj;
    float4   color;
    float4   lightDir;
    float4   lightColor;
    float4   params;      // x=ambient, y=intensity, z=shadowEnabled
    float4   cameraPos;   // xyz=camera world position
    float4   shading;     // x=mode (0=Lit, 1=Unlit, 2=Toon), y=receiveShadow, z=metallic
    float4   specRim;     // x=specularStrength, y=roughness, z=rimStrength, w=rimPower
    float4   rimColor;    // rgb=rim light color
    float4   toon;        // x=threshold, y=softness, z=texture steps, w=texture smooth radius (UV)
    float4   toonShade;   // rgb=color multiplied on the shaded side
    float4   outline;     // rgb=outline color, a=width in world units
};
Texture2D    tex        : register(t0);
Texture2D    shadowMap  : register(t1);
SamplerState samp       : register(s0);
SamplerComparisonState shadowSamp : register(s1);

struct PSIn { float4 pos:SV_POSITION; float3 nrm:NORMAL; float4 col:COLOR; float2 uv:TEXCOORD; float3 wpos:WPOS; };

// ---- Vertex shader helpers ----
// Builds the vertex output from a world position and a world normal.
PSIn MakePSIn(float3 wpos, float3 wnrm, float4 col, float2 uv) {
    PSIn o;
    o.wpos = wpos;
    o.pos  = mul(float4(wpos, 1.0), viewProj);
    o.nrm  = wnrm;
    o.col  = col;
    o.uv   = uv;
    return o;
}

// Outline (inverted hull): push the vertex outward along its normal.
// The pass draws back faces only, so the enlarged shape shows just around the silhouette.
PSIn MakeOutlinePSIn(float3 wpos, float3 wnrm, float4 col, float2 uv) {
    float  len = length(wnrm);
    float3 n   = (len > 1e-5) ? (wnrm / len) : float3(0.0, 0.0, 0.0);
    return MakePSIn(wpos + n * outline.a, n, col, uv);
}

// ---- Pixel shader ----
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

// Turns a smooth 0..1 value into "almost 0 or 1" around the edge value.
// width is the half width of the blurred border.
float ToonStep(float value, float edge, float width) {
    return smoothstep(edge - width, edge + width, value);
}

// Toon only: makes a detailed (photo-like) texture look painted.
//   1. Smooth : average with 8 neighbours on a circle of radius toon.w (in UV), which removes fine detail.
//   2. Steps  : round the brightness to toon.z levels. The hue is kept, so colors do not shift.
// Both are skipped when their value is 0.
float3 SimplifyTexture(float2 uv, float3 texColor) {
    float radius = toon.w;
    if (radius > 0.0) {
        static const float2 kOffsets[8] = {
            float2( 1.0,  0.0),   float2(-1.0,  0.0),   float2( 0.0,  1.0),   float2( 0.0, -1.0),
            float2( 0.707, 0.707), float2(-0.707, 0.707), float2( 0.707,-0.707), float2(-0.707,-0.707)
        };
        float3 sum = texColor;
        [unroll] for (int k = 0; k < 8; ++k)
            sum += tex.SampleLevel(samp, uv + kOffsets[k] * radius, 0).rgb;
        texColor = sum / 9.0;
    }

    float steps = toon.z;
    if (steps >= 1.5) {
        float v = max(texColor.r, max(texColor.g, texColor.b));   // brightness
        if (v > 1e-4) {
            float q = max(floor(v * steps + 0.5), 1.0) / steps;   // rounded brightness (never fully black)
            texColor *= q / v;
        }
    }
    return texColor;
}

// ---- Material: metallic / roughness ----
// Sky colors used for reflections. Same values as the sky drawn by EngineSkybox.cpp: keep them in sync.
static const float3 kSkyHorizon = float3(0.55, 0.65, 0.78);
static const float3 kSkyZenith  = float3(0.13, 0.28, 0.55);
static const float3 kSkyGround  = float3(0.18, 0.20, 0.23);
static const float3 kSkyAverage = (kSkyHorizon * 2.0 + kSkyZenith + kSkyGround) * 0.25;

float3 SkyColor(float3 dir) {
    return (dir.y >= 0.0) ? lerp(kSkyHorizon, kSkyZenith, saturate(dir.y))
                          : lerp(kSkyHorizon, kSkyGround, saturate(-dir.y));
}

// Blinn-Phong exponent from roughness (0.5 gives about 30). Smooth = small sharp highlight.
float SpecularPower(float rough) {
    float a = max(rough * rough, 0.03);
    return clamp(2.0 / (a * a) - 2.0, 1.0, 2048.0);
}

// Color of one pixel of a surface (lighting, toon, material). Used by PSMain below.
float3 ShadeSurface(PSIn i) {
    // mode must match ShadingMode in ShadingParams.h
    int mode = (int)(shading.x + 0.5);

    float4 t = tex.Sample(samp, i.uv);
    if (mode == 2) t.rgb = SimplifyTexture(i.uv, t.rgb);
    float3 base = t.rgb * i.col.rgb * color.rgb;

    if (mode == 1) return base;   // Unlit: texture * color only

    float3 n = normalize(i.nrm);
    float3 l = normalize(-lightDir.xyz);            // surface -> light
    float3 v = normalize(cameraPos.xyz - i.wpos);   // surface -> camera
    float3 h = normalize(l + v);
    float3 r = reflect(-v, n);                      // direction seen in a mirror

    // Material. A metal has no base (diffuse) color of its own:
    // what you see is the highlight and the reflection, both tinted by the base color.
    float  metallic   = saturate(shading.z);
    float  rough      = saturate(specRim.y);
    float3 diffuseCol = base * (1.0 - metallic);
    float3 specCol    = lerp(float3(1.0, 1.0, 1.0), base, metallic);

    // Values shared by Lit and Toon (all smooth, 0..1 except ndl)
    float ndl     = dot(n, l);
    float sh      = (params.z > 0.5 && shading.y > 0.5) ? ShadowFactor(i.wpos) : 1.0;
    float specRaw = pow(saturate(dot(n, h)), SpecularPower(rough));         // Blinn-Phong
    float rimRaw  = pow(1.0 - saturate(dot(n, v)), max(specRim.w, 0.01));   // bright near the silhouette

    float3 c;
    if (mode == 2) {
        // ---- Toon: split into a lit side and a shaded side ----
        float w   = max(toon.y, 0.001);
        float hl  = ndl * 0.5 + 0.5;                              // half Lambert, 0..1
        float lit = ToonStep(hl, toon.x, w) * ToonStep(sh, 0.5, w);   // cast shadows become the shaded side too

        // Brightest value is the same as Lit (ambient + intensity), capped so colors do not blow out.
        float3 lightCol = lightColor.rgb * saturate(params.x + params.y);
        c = diffuseCol * lightCol * lerp(toonShade.rgb, float3(1.0, 1.0, 1.0), lit);

        // Specular and rim become hard-edged shapes. Roughness / rim power control their size.
        c += lightColor.rgb * specCol * (ToonStep(specRaw, 0.5, w) * specRim.x * lit);
        c += rimColor.rgb * (ToonStep(rimRaw, 0.5, w) * specRim.z);

        // Metal reflection, toon style: sky above / ground below with a hard horizon line.
        // A rough surface blurs that line.
        float  up  = ToonStep(r.y, 0.0, max(w, rough * 0.5));
        float3 env = lerp(kSkyGround, lerp(kSkyHorizon, kSkyZenith, 0.5), up);
        c += base * env * metallic;
    }
    else {
        // ---- Lit: ambient + directional light * shadow ----
        float d = saturate(ndl);
        c = diffuseCol * (params.x + d * params.y * sh) * lightColor.rgb;

        // Specular is added on top. Hidden in shadow and on faces that the light does not reach.
        c += lightColor.rgb * specCol * (specRaw * specRim.x * smoothstep(0.0, 0.1, d) * sh * params.y);

        // Rim depends on the view only, so it also shows in shadow.
        c += rimColor.rgb * (rimRaw * specRim.z);

        // Metal reflection: the sky gradient seen in the mirror direction.
        // A rough surface blurs it toward the average sky color.
        float3 env = lerp(SkyColor(r), kSkyAverage, rough);
        c += base * env * metallic;
    }
    return c;
}

// ---- Pixel shader outputs ----
// The scene is drawn into two render targets at once:
//   target 0 = color, target 1 = world normal (read later by the post process to find edges).
// When only the color target is bound, the second output is simply dropped.
struct PSOut { float4 color : SV_TARGET0; float4 normal : SV_TARGET1; };

PSOut MakePSOut(float3 c, float3 worldNormal) {
    PSOut o;
    o.color  = float4(c, 1.0);
    o.normal = float4(normalize(worldNormal) * 0.5 + 0.5, 1.0);   // a = 1 marks "something was drawn here"
    return o;
}

PSOut PSMain(PSIn i)    { return MakePSOut(ShadeSurface(i), i.nrm); }

// Outline pass: a single flat color, no lighting.
PSOut PSOutline(PSIn i) { return MakePSOut(outline.rgb, i.nrm); }
)";

    // HLSL をコンパイルする。失敗したら、エラー内容を Visual Studio の「出力」ウィンドウに出して false を返す。
    //   name   : エラー表示に使う名前（"mesh" など）
    //   entry  : 開始関数名（"VSMain" / "PSMain" など）
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