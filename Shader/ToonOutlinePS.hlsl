#include "Pbr.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    // ★ アウトラインの色（少しだけ明るい黒にすると馴染みます）
    return float4(0.05f, 0.05f, 0.05f, 1.0f);
}