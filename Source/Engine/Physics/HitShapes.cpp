#include "Engine/Physics/HitShapes.h"
#include <cmath>
#include <cfloat>

using namespace DirectX;

namespace uel {
    namespace {
        float Dot(const XMFLOAT3& a, const XMFLOAT3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    }

    bool RaySphere(const XMFLOAT3& o, const XMFLOAT3& d, const XMFLOAT3& c, float r, float& tOut)
    {
        XMFLOAT3 oc = Sub(o, c);
        float b = Dot(oc, d);
        float k = Dot(oc, oc) - r * r;
        float h = b * b - k;
        if (h < 0.0f) return false;
        h = sqrtf(h);
        float t = -b - h;
        if (t < 0.0f) t = -b + h;      // 始点が球の内側
        if (t < 0.0f) return false;
        tOut = t;
        return true;
    }

    bool RayCapsule(const XMFLOAT3& o, const XMFLOAT3& d, const XMFLOAT3& a, const XMFLOAT3& b, float r, float& tOut)
    {
        float best = FLT_MAX;
        bool  hit = false;

        // 円柱部分：軸（a→b）からの距離が r になる t を二次方程式で求める
        XMFLOAT3 ba = Sub(b, a), oa = Sub(o, a);
        float baba = Dot(ba, ba), bard = Dot(ba, d), baoa = Dot(ba, oa);
        float rdoa = Dot(d, oa), oaoa = Dot(oa, oa);
        float k2 = baba - bard * bard;
        if (k2 > 1e-8f) {                                   // レイが軸と平行なら円柱部は無視（両端の球で判定）
            float k1 = baba * rdoa - baoa * bard;
            float k0 = baba * oaoa - baoa * baoa - r * r * baba;
            float h = k1 * k1 - k2 * k0;
            if (h >= 0.0f) {
                float t = (-k1 - sqrtf(h)) / k2;
                float y = baoa + t * bard;                  // 当たった点の軸方向の位置
                if (t >= 0.0f && y > 0.0f && y < baba) { best = t; hit = true; }
            }
        }

        // 両端の半球
        float ts;
        if (RaySphere(o, d, a, r, ts) && ts < best) { best = ts; hit = true; }
        if (RaySphere(o, d, b, r, ts) && ts < best) { best = ts; hit = true; }

        if (hit) tOut = best;
        return hit;
    }
}