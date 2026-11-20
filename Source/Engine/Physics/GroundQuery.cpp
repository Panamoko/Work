#include "Engine/Physics/GroundQuery.h"
#include "Engine/Physics/MeshBVH.h"
#include "Engine/Graphics/Mesh.h"
#include <cfloat>
#include <cmath>
#include <memory>
#include <unordered_map>

using namespace DirectX;

namespace uel {
    namespace {
        // メッシュごとの BVH（5-4a）。そのメッシュを初めて調べるときに作り、後は使い回す。
        // メッシュは MeshCache がアプリの終わりまで持っているので、Mesh のアドレスをキーにしてよい
        // （中身が入れ替わったら IsBuiltFor が false になり、作り直す）
        const MeshBVH& BVHFor(const Mesh& mesh)
        {
            static std::unordered_map<const Mesh*, std::unique_ptr<MeshBVH>> cache;
            std::unique_ptr<MeshBVH>& p = cache[&mesh];
            if (!p) p = std::make_unique<MeshBVH>();
            if (!p->IsBuiltFor(mesh)) p->Build(mesh);
            return *p;
        }

        // ワールド行列の逆。大きさ 0 などで逆が無ければ false（そのメッシュには当たらない）
        bool Inverse(const XMMATRIX& world, XMMATRIX& inv)
        {
            XMVECTOR det;
            inv = XMMatrixInverse(&det, world);
            const float d = XMVectorGetX(det);
            return std::isfinite(d) && std::fabs(d) > 1e-12f;
        }
    }

    // 真上向きの線（ワールドの (x, *, z)）をメッシュの空間へ移して、いちばん高い当たりを BVH で探す。
    // ワールドで (x, 0, z) から (0, 1, 0) 向きの線は、移した後も「t = ワールドの高さ」のまま（行列は線の t を変えない）
    bool RaycastGroundColumn(const Mesh& mesh, const XMMATRIX& world, float x, float z, float& outY, float maxY)
    {
        XMMATRIX inv;
        if (!Inverse(world, inv)) return false;
        XMFLOAT3 o, d;
        XMStoreFloat3(&o, XMVector3TransformCoord(XMVectorSet(x, 0.0f, z, 1.0f), inv));
        XMStoreFloat3(&d, XMVector3TransformNormal(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), inv));
        float t;
        if (!BVHFor(mesh).LineHighest(mesh, o, d, maxY, t)) return false;
        outY = t;
        return true;
    }

    // ワールドの線（向きは長さ 1 にそろえる）をメッシュの空間へ移して、いちばん近い当たりを BVH で探す。
    // 向きは長さ 1 に直さずに移すので、見つかった t はそのままワールドの距離（今までと同じ意味）
    bool RaycastMesh(const Mesh& mesh, const XMMATRIX& world,
        const XMFLOAT3& origin, const XMFLOAT3& dir, float maxDist, float& outDist)
    {
        XMMATRIX inv;
        if (!Inverse(world, inv)) return false;
        XMFLOAT3 o, d;
        XMStoreFloat3(&o, XMVector3TransformCoord(XMLoadFloat3(&origin), inv));
        XMStoreFloat3(&d, XMVector3TransformNormal(XMVector3Normalize(XMLoadFloat3(&dir)), inv));
        float t;
        if (!BVHFor(mesh).RayClosest(mesh, o, d, 1e-6f, maxDist, t)) return false;
        outDist = t;
        return true;
    }
} // namespace uel
