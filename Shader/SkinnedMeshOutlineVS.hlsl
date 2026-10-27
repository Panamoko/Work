#include "SkinnedVertex.hlsli"

// スキンドメッシュ：アウトライン（背面法）。スキニング後の形を法線の向きに押し出す
PSIn main(VSIn i)
{
    float3 p;
    float3 n;
    ToWorld(i, p, n);
    return MakeOutlinePSIn(p, n, i.col, i.uv);
}
