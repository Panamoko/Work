#include "PostProcess.hlsli"

// ポストプロセス：画面全体を覆う大きな三角形を1枚描く（空と同じ方法）。
// 頂点バッファは使わず、頂点番号（0, 1, 2）から位置を作る（C++ 側は Draw(3, 0) だけ）。
//   id 0 → uv (0, 0) 左上 / id 1 → uv (2, 0) / id 2 → uv (0, 2)
PostVSOut main(uint id : SV_VertexID)
{
    PostVSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.uv = uv;
    return o;
}
