#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cstdint>

namespace uel {

    struct SkeletonBone {
        std::string name;
        int parent = -1;                       // bones配列内の親index（-1=ルート）
        DirectX::XMFLOAT4X4 inverseBind{};      // バインド時 world->bone（行ベクトル形）
        DirectX::XMFLOAT3 t{ 0,0,0 };           // ローカルbind TRS
        DirectX::XMFLOAT4 r{ 0,0,0,1 };         // クォータニオン xyzw
        DirectX::XMFLOAT3 s{ 1,1,1 };
    };

    struct Skeleton {
        std::vector<SkeletonBone> bones;
        bool Empty() const { return bones.empty(); }
        int Find(const std::string& n) const {
            for (int i = 0; i < (int)bones.size(); ++i) if (bones[i].name == n) return i;
            return -1;
        }
    };

} // namespace uel