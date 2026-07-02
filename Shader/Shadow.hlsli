#ifndef SHADOW_HLSLI
#define SHADOW_HLSLI

#include "Scene.hlsli" // lightViewProjection, shadowParams

Texture2D ShadowMap : register(t8);
SamplerComparisonState ShadowSampler : register(s2);

// Percentage-closer filtered shadow lookup.
// Returns 0 = fully shadowed, 1 = fully lit (PCF 3x3).
float ComputeShadow(float3 worldPos)
{
    if (shadowParams.z < 0.5f)
        return 1.0f;

    float4 lp = mul(float4(worldPos, 1.0f), lightViewProjection);
    lp.xyz /= lp.w;
    if (lp.z > 1.0f || lp.z < 0.0f)
        return 1.0f;

    float2 uv = lp.xy * float2(0.5f, -0.5f) + 0.5f;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    float depth = lp.z - shadowParams.y;
    float texel = shadowParams.w;

    float s = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; y++)
        [unroll]
        for (int x = -1; x <= 1; x++)
            s += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + float2(x, y) * texel, depth);
    return s / 9.0f;
}

#endif // SHADOW_HLSLI
