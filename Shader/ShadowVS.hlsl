#include "Shadow.hlsli"

// 影（静的メッシュ）：頂点をライトの視点へ移す。読むのは位置だけ（入力レイアウトも POSITION だけ）。
float4 main(float3 pos : POSITION) : SV_POSITION
{
    float4 wp = mul(float4(pos, 1.0), world);
    return mul(wp, lightViewProj);
}
