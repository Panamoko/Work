#include "Lighting.hlsli"

// メッシュの表面（静的・スキンドで共用）：色と法線を書く
PSOut main(PSIn i)
{
    return MakePSOut(ShadeSurface(i), i.nrm);
}
