#include "Engine/Physics/GroundQuery.h"
#include "Engine/Graphics/Mesh.h"
#include <cfloat>

using namespace DirectX;

namespace uel {
    namespace {
        // 三角形をXZに射影して (x,z) が内側なら、その点のYを補間して返す
        bool TriGroundY(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, float x, float z, float& y)
        {
            float d = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
            if (fabsf(d) < 1e-8f) return false;
            float u = ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) / d;
            float v = ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) / d;
            float w = 1.0f - u - v;
            if (u < 0.0f || v < 0.0f || w < 0.0f) return false;
            y = u * a.y + v * b.y + w * c.y;
            return true;
        }
    }

    bool RaycastGroundColumn(const Mesh& mesh, const XMMATRIX& world, float x, float z, float& outY)
    {
        const auto& pos = mesh.Positions();
        const auto& idx = mesh.Indices();
        bool found = false;
        float best = -FLT_MAX;

        for (size_t i = 0; i + 3 <= idx.size(); i += 3) {
            XMFLOAT3 w0, w1, w2;
            XMStoreFloat3(&w0, XMVector3TransformCoord(XMLoadFloat3(&pos[idx[i + 0]]), world));
            XMStoreFloat3(&w1, XMVector3TransformCoord(XMLoadFloat3(&pos[idx[i + 1]]), world));
            XMStoreFloat3(&w2, XMVector3TransformCoord(XMLoadFloat3(&pos[idx[i + 2]]), world));
            float y;
            if (TriGroundY(w0, w1, w2, x, z, y)) {
                if (!found || y > best) { best = y; found = true; }
            }
        }
        if (found) outY = best;
        return found;
    }

    bool RaycastMesh(const Mesh& mesh, const XMMATRIX& world,
        const XMFLOAT3& origin, const XMFLOAT3& dir, float maxDist, float& outDist)
    {
        const auto& pos = mesh.Positions();
        const auto& idx = mesh.Indices();
        XMVECTOR O = XMLoadFloat3(&origin);
        XMVECTOR D = XMVector3Normalize(XMLoadFloat3(&dir));
        const float EPS = 1e-6f;
        bool found = false;
        float best = maxDist;

        for (size_t i = 0; i + 3 <= idx.size(); i += 3) {
            XMVECTOR v0 = XMVector3TransformCoord(XMLoadFloat3(&pos[idx[i + 0]]), world);
            XMVECTOR v1 = XMVector3TransformCoord(XMLoadFloat3(&pos[idx[i + 1]]), world);
            XMVECTOR v2 = XMVector3TransformCoord(XMLoadFloat3(&pos[idx[i + 2]]), world);
            XMVECTOR e1 = XMVectorSubtract(v1, v0);
            XMVECTOR e2 = XMVectorSubtract(v2, v0);
            XMVECTOR h = XMVector3Cross(D, e2);
            float a = XMVectorGetX(XMVector3Dot(e1, h));
            if (a > -EPS && a < EPS) continue;
            float f = 1.0f / a;
            XMVECTOR s = XMVectorSubtract(O, v0);
            float u = f * XMVectorGetX(XMVector3Dot(s, h));
            if (u < 0.0f || u > 1.0f) continue;
            XMVECTOR q = XMVector3Cross(s, e1);
            float v = f * XMVectorGetX(XMVector3Dot(D, q));
            if (v < 0.0f || u + v > 1.0f) continue;
            float t = f * XMVectorGetX(XMVector3Dot(e2, q));
            if (t > EPS && t < best) { best = t; found = true; }
        }
        if (found) outDist = best;
        return found;
    }
} // namespace uel