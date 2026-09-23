#pragma once
#include <DirectXMath.h>

namespace uel {
    class Mesh;

    // (x,z) の真下にあるメッシュ面の最も高いYを返す（見つかれば true）。
    bool RaycastGroundColumn(const Mesh& mesh, const DirectX::XMMATRIX& world,
        float x, float z, float& outY);
}