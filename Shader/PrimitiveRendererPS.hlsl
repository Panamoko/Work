#include "PrimitiveRenderer.hlsli"

// ★追加：C++から送られてくるテクスチャとサンプラ
Texture2D colorMap : register(t0);
SamplerState colorSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 finalColor = pin.color;

    // ★追加：テクスチャ使用フラグが立っていれば、画像の色を掛け合わせる
    if (useTexture)
    {
        float4 texColor = colorMap.Sample(colorSampler, pin.uv);
        finalColor *= texColor;
    }

    // ★プロの安全装置：完全に透明なピクセルは描画処理を破棄（Discard）する
    if (finalColor.a <= 0.0f)
    {
        discard;
    }

    return finalColor;
}