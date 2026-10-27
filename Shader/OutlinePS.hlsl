#include "Common.hlsli"

// アウトライン（背面法、静的・スキンドで共用）：ライティングなしの単色
PSOut main(PSIn i)
{
    return MakePSOut(outline.rgb, i.nrm);
}
