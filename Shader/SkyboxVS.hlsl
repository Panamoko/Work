#include "Skybox.hlsli"

// 空：画面全体を覆う大きな三角形を1枚描く。
// 頂点バッファは使わず、頂点番号（0, 1, 2）から位置を作る（C++ 側は Draw(3, 0) だけ）。
//   id 0 → (-1,  1) 左上 / id 1 → ( 3,  1) / id 2 → (-1, -3)
//   画面（-1〜1 の四角）は、この三角形にすっぽり入る。
SkyVSOut main(uint id : SV_VertexID)
{
    SkyVSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.ndc = o.pos.xy;
    return o;
}
