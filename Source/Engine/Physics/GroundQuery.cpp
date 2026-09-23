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

} // namespace uel