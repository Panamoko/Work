#include "PrimitiveRenderer.hlsli"

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;
    
    // float3の位置ベクトルをfloat4に変換して行列と掛け合わせる
    vout.position = mul(float4(vin.position, 1.0f), viewProjection);
    
    vout.color = vin.color;
    vout.uv = vin.uv; // ★UV座標を次の処理へ渡す

    return vout;
}