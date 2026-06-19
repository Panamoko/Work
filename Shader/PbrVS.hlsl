#include "Skinning.hlsli"
#include "Pbr.hlsli"

VS_OUT main(
    float4 position : POSITION,
    float3 normal : NORMAL,
    float4 tangent : TANGENT,
    float2 texcoord : TEXCOORD,
    float4 boneWeights : BONE_WEIGHTS,
    uint4 boneIndices : BONE_INDICES)
{
    VS_OUT vout = (VS_OUT) 0;

    float4 worldPosition = SkinningPosition(position, boneWeights, boneIndices);

    vout.worldPosition = worldPosition.xyz;
    vout.position = mul(worldPosition, viewProjection);
    vout.normal = normalize(SkinningVector(normal, boneWeights, boneIndices));
    vout.tangent = float4(normalize(SkinningVector(tangent.xyz, boneWeights, boneIndices)), tangent.w);
    vout.texcoord = texcoord;

    return vout;
}