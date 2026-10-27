#ifndef MESH_VERTEX_HLSLI
#define MESH_VERTEX_HLSLI

#include "Common.hlsli"

// 静的メッシュの頂点（C++ の MeshVertex と、MeshShader.cpp の入力レイアウトに合わせる）
struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float4 col : COLOR;
    float2 uv : TEXCOORD;
};

// ワールド位置とワールド法線を求める。通常用（MeshVS）とアウトライン用（MeshOutlineVS）で共通。
void ToWorld(VSIn i, out float3 wpos, out float3 wnrm)
{
    wpos = mul(float4(i.pos, 1.0), world).xyz;
    wnrm = mul(i.nrm, (float3x3) world);
}

#endif // MESH_VERTEX_HLSLI
