#include "Skybox.hlsli"
#include "SkyColor.hlsli"

// 空：ピクセルごとに「カメラからその方向を見たときの空の色」を出す。
float4 main(SkyVSOut i) : SV_TARGET
{
    // 画面上の点を、いちばん奥（深度 1）にあるワールドの点に戻し、カメラからの向きにする
    float4 world = mul(float4(i.ndc, 1.0, 1.0), invViewProj);
    float3 dir = normalize(world.xyz / world.w - camPos.xyz);

    // 空の画像があれば、回転に合わせた向きの色を読んで、画面の色に直す。無ければグラデーション
    if (camPos.w > 0.5)
        return float4(HdrToScreen(skyTex.Sample(skySamp, RotateSkyDir(dir, skyParams.yz)).rgb, skyParams.x), 1.0);
    return float4(SkyColor(dir), 1.0);
}