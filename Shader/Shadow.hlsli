#ifndef SHADOW_HLSLI
#define SHADOW_HLSLI

// シャドウマップを作るパス（ライトから見た深度を書く）で共通に使う宣言。
// 深度だけを書くので、ピクセルシェーダは無い（C++ 側で PSSetShader(nullptr)）。

// b0：C++ 側（EngineShadowMap.cpp の SCB）と、並び・型を必ず一致させること。
cbuffer ShadowCB : register(b0)
{
    float4x4 lightViewProj; // ライトから見たビュー × 射影（カメラに追従する平行投影）
    float4x4 world;
};

#endif // SHADOW_HLSLI
