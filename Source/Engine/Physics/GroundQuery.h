#pragma once
#include <DirectXMath.h>

namespace uel {
    class Mesh;

    bool RaycastGroundColumn(const Mesh& mesh, const DirectX::XMMATRIX& world,
        float x, float z, float& outY);

    // origin から dir 方向のレイ。最も近い三角形までの距離を outDist に（maxDist 未満のみ）。
    bool RaycastMesh(const Mesh& mesh, const DirectX::XMMATRIX& world,
        const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir,
        float maxDist, float& outDist);
}