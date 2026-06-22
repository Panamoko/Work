#include "Skinning.hlsli"
#include "Pbr.hlsli"

cbuffer CbOutline : register(b0)
{
    float outlineWidth;
    float3 pad;
};

VS_OUT main(
    float4 position : POSITION,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 texcoord : TEXCOORD,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES)
{
    VS_OUT vout = (VS_OUT) 0;

    // スキニング後の座標と法線を計算
    float4 worldPosition = SkinningPosition(position, boneWeights, boneIndices);
    float3 worldNormal = normalize(SkinningVector(normal, boneWeights, boneIndices));

    // ★ 頂点を法線の方向（外側）に少しだけ押し出す（太らせる）
    worldPosition.xyz += worldNormal * outlineWidth;

    vout.worldPosition = worldPosition.xyz;
    vout.position = mul(worldPosition, viewProjection);
    vout.normal = worldNormal;
    vout.tangent = tangent;
    vout.texcoord = texcoord;

    return vout;
}