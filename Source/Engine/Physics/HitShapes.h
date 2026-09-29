#pragma once
#include <DirectXMath.h>

namespace uel {
    // レイ（d は正規化済み）と球・カプセルの交差。当たれば始点からの距離 t を返す
    bool RaySphere(const DirectX::XMFLOAT3& o, const DirectX::XMFLOAT3& d,
        const DirectX::XMFLOAT3& c, float r, float& tOut);
    bool RayCapsule(const DirectX::XMFLOAT3& o, const DirectX::XMFLOAT3& d,
        const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float r, float& tOut);
}