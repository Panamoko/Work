#include "SkinnedVertex.hlsli"

// スキンドメッシュ：通常の描画
PSIn main(VSIn i)
{
    float3 p;
    float3 n;
    ToWorld(i, p, n);
    return MakePSIn(p, n, i.col, i.uv);
}
