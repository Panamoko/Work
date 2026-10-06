#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cstdint>

namespace uel {

    // ボーン = glTF のノード1つ。スキンのジョイントだけでなく、その親ノード（Armature など）や
    // スキンを持たないメッシュのノードも入る。
    struct SkeletonBone {
        std::string name;
        int parent = -1;                        // bones 配列内の親 index（-1 = ルート）。必ず自分より前にある
        DirectX::XMFLOAT3 t{ 0,0,0 };           // ローカル bind TRS
        DirectX::XMFLOAT4 r{ 0,0,0,1 };         // クォータニオン xyzw
        DirectX::XMFLOAT3 s{ 1,1,1 };
    };

    // 頂点が参照する単位。「どのボーンに」「どの逆バインド行列で」付くかの組。
    // 同じボーンでも、スキンごとに逆バインド行列が違うモデルがあるので、ボーンとは別に持つ。
    struct SkinJoint {
        int bone = 0;                           // skeleton.bones の index
        DirectX::XMFLOAT4X4 inverseBind{};      // メッシュ空間 -> ボーン空間（行ベクトル形式）
    };

    struct Skeleton {
        std::vector<SkeletonBone> bones;        // 親が先に並ぶ
        std::vector<SkinJoint>    joints;       // SkinnedVertex::bones はここへの index。パレットはこの数だけ作る
        bool Empty() const { return bones.empty(); }
        int Find(const std::string& n) const {
            for (int i = 0; i < (int)bones.size(); ++i) if (bones[i].name == n) return i;
            return -1;
        }
    };

} // namespace uel