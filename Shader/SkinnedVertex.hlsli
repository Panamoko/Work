#ifndef SKINNED_VERTEX_HLSLI
#define SKINNED_VERTEX_HLSLI

#include "Common.hlsli"
#include "Skinning.hlsli"

// スキンドメッシュの頂点（C++ の SkinnedVertex 80 バイトと、SkinnedMeshShader.cpp の入力レイアウトに合わせる）
struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float4 col : COLOR;
    float2 uv : TEXCOORD;
    uint4 bidx : BLENDINDICES; // 影響するボーンの番号（4本まで）
    float4 bw : BLENDWEIGHT; // それぞれの重み
};

// スキニングしてから、ワールド位置とワールド法線を求める。通常用とアウトライン用で共通。
void ToWorld(VSIn i, out float3 wpos, out float3 wnrm)
{
    float3 sp;
    float3 sn;
    Skin(i.pos, i.nrm, i.bidx, i.bw, sp, sn);
    wpos = mul(float4(sp, 1.0), world).xyz;
    wnrm = mul(sn, (float3x3) world);
}

#endif // SKINNED_VERTEX_HLSLI
