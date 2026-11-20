#pragma once
#include <DirectXMath.h>
#include <cfloat>

namespace uel {
    class Mesh;

    // メッシュへの線の当たり（5-4a から、中身はメッシュごとの BVH で速く調べる。Physics/MeshBVH）。
    // どちらも、そのメッシュを初めて調べるときに BVH を作る（MP_Island の 72 万個で Release 約 0.数秒）。

    // ワールドの (x, z) を通る真上向きの線で、いちばん高い面の高さ（Y）を outY に。
    // maxY を渡すと、その高さより上の面は見ない（木の葉や橋の下の地面を探すとき用。省くと今までどおり上限なし）
    bool RaycastGroundColumn(const Mesh& mesh, const DirectX::XMMATRIX& world,
        float x, float z, float& outY, float maxY = FLT_MAX);

    // origin から dir 方向のレイ。最も近い三角形までの距離を outDist に（maxDist 未満のみ）。
    bool RaycastMesh(const Mesh& mesh, const DirectX::XMMATRIX& world,
        const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir,
        float maxDist, float& outDist);
}
