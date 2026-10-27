#include "MeshVertex.hlsli"

// 静的メッシュ：アウトライン（背面法）。法線の向きに押し出した形を作る
PSIn main(VSIn i)
{
    float3 p;
    float3 n;
    ToWorld(i, p, n);
    return MakeOutlinePSIn(p, n, i.col, i.uv);
}
