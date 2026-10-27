#ifndef SKY_COLOR_HLSLI
#define SKY_COLOR_HLSLI

// 空の色（上下のグラデーション）。
// 空の描画（SkyboxPS.hlsl）と、金属の映り込み（メッシュのシェーダ）で同じ色を使うため、ここに1つだけ書く。

static const float3 kSkyHorizon = float3(0.55, 0.65, 0.78); // 地平線
static const float3 kSkyZenith = float3(0.13, 0.28, 0.55); // 真上
static const float3 kSkyGround = float3(0.18, 0.20, 0.23); // 真下（地面側）

// 見ている向き dir（長さ 1）の空の色。上を向くほど真上の色、下を向くほど地面側の色に近づく。
float3 SkyColor(float3 dir)
{
    return (dir.y >= 0.0) ? lerp(kSkyHorizon, kSkyZenith, saturate(dir.y))
                          : lerp(kSkyHorizon, kSkyGround, saturate(-dir.y));
}

#endif // SKY_COLOR_HLSLI
