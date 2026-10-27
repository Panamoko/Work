#include "Shadow.hlsli"
#include "Skinning.hlsli"

// 影（スキンドメッシュ）：本描画と同じ姿勢にスキニングしてから、ライトの視点へ移す。
// 読むのは位置・ボーン番号・重みだけ（SkinnedVertex 80 バイトのうち 0 / 48 / 64 バイト目）。
struct ShadowSkinnedIn
{
    float3 pos : POSITION;
    uint4 bidx : BLENDINDICES;
    float4 bw : BLENDWEIGHT;
};

float4 main(ShadowSkinnedIn i) : SV_POSITION
{
    float3 sp;
    float3 unusedNormal; // 影には法線が要らない（計算はコンパイラが省く）
    Skin(i.pos, float3(0.0, 0.0, 0.0), i.bidx, i.bw, sp, unusedNormal);
    float4 wp = mul(float4(sp, 1.0), world);
    return mul(wp, lightViewProj);
}
