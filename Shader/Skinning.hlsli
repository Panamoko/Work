#ifndef SKINNING_HLSLI
#define SKINNING_HLSLI

// スキニング（ボーンの行列で頂点を動かす）。本描画（SkinnedVertex.hlsli）と影（ShadowSkinnedVS.hlsl）で共通。

// b1：ボーンの行列（palette）。C++ 側の BoneCB（SkinnedMeshShader.cpp と EngineShadowMap.cpp、kMaxBones = 128）と合わせる。
cbuffer Bones : register(b1)
{
    float4x4 bones[128];
};

// 位置 pos と法線 nrm を、最大4本のボーン（番号 bidx、重み bw）の重み付き平均で動かす。
// 重みが1つも無い頂点は、そのままの位置・向きにする。
// 法線を使わない呼び出し側（影）では、法線の計算はコンパイラが省く。
void Skin(float3 pos, float3 nrm, uint4 bidx, float4 bw, out float3 skinnedPos, out float3 skinnedNrm)
{
    float4 p = float4(pos, 1.0);
    float3 sp = 0;
    float3 sn = 0;
    float wsum = 0;
    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        float w = bw[k];
        uint b = bidx[k];
        sp += w * mul(p, bones[b]).xyz;
        sn += w * mul(nrm, (float3x3) bones[b]);
        wsum += w;
    }
    if (wsum < 1e-4)
    {
        sp = pos;
        sn = nrm;
    }
    skinnedPos = sp;
    skinnedNrm = sn;
}

#endif // SKINNING_HLSLI
