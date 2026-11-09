#pragma once
#include <DirectXMath.h>
#include "Engine/Graphics/ShadingParams.h"

// 静的メッシュ（MeshShader）とスキンドメッシュ（SkinnedMeshShader）で共通に使う、C++ 側の部分。
//   ・b0 の定数バッファ ObjectCB の形
//   ・ShadingParams（描き方のデータ）を ObjectCB に詰める関数
// シェーダ本体（HLSL）は Shader/ フォルダにある：
//   Common.hlsli    … b0 の宣言、頂点 → ピクセルの受け渡し、出力の形
//   Lighting.hlsli  … 影・Toon・材質など、1ピクセルの色の計算（ShadeSurface）
//   MeshVertex.hlsli / SkinnedVertex.hlsli … 頂点の形とワールドへの変換
//   MeshVS / MeshOutlineVS / SkinnedMeshVS / SkinnedMeshOutlineVS / SurfacePS / OutlinePS（.hlsl）
// 見た目の式を変えるときは Lighting.hlsli を直せば、静的・スキンドの両方に効く。

namespace uel {

    // b0：オブジェクト＋ライト＋描き方。
    // Shader/Common.hlsli の「cbuffer ObjectCB」と、並び・型を必ず一致させること。
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
        DirectX::XMFLOAT4   sky;             // x=空の明るさ, y=cos(空の回転), z=sin(空の回転)（SkyShaderParams で作る）
    };

    // ShadingParams（描き方のデータ）を定数バッファに詰める。
    // 項目を増やすときは、ここ・ObjectCB・Shader/Common.hlsli の cbuffer・Shader/Lighting.hlsli の式をそろえる。
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

} // namespace uel
