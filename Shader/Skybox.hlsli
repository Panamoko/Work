#ifndef SKYBOX_HLSLI
#define SKYBOX_HLSLI

// 空（スカイボックス）の VS と PS で共通に使う宣言。

// b0：C++ 側（EngineSkybox.cpp の SkyCB）と、並び・型を必ず一致させること。
cbuffer SkyCB : register(b0)
{
    float4x4 invViewProj; // （ビュー × 射影）の逆行列。画面上の点 → ワールドの点
    float4 camPos; // xyz = カメラのワールド位置、w = 1 なら空の画像（skyTex）を使う / 0 ならグラデーション
};

TextureCube skyTex : register(t0); // 空の画像（キューブマップ。HDR：明るさに上限が無い）
SamplerState skySamp : register(s0); // 線形補間・端で止める

struct SkyVSOut
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD; // 画面上の位置（左下 -1 〜 右上 1）
};

#endif // SKYBOX_HLSLI