#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace uel {
    class Mesh;

    // メッシュの三角形を「箱の木」（BVH：Bounding Volume Hierarchy）に整理して、線と三角形の当たりを速く調べる（5-4a）。
    // 今までは1回調べるたびに全部の三角形を見ていた（MP_Island の 72 万個で、Release でも 1 回 13〜16ms）。
    // BVH なら、線が通らない箱の中身は見ないので、見る三角形は数十個で済む。
    // UE の物理（Chaos）が静的メッシュの当たり判定に使う「三角形メッシュの BVH」と同じ考え方。
    //
    // ・作るのはメッシュの空間（Transform を掛ける前）。調べるときは、線のほうをメッシュの空間へ移す
    //   （三角形を毎回ワールドへ移さない。Transform が変わっても作り直さなくてよい）。
    // ・作るのは、そのメッシュを初めて調べるとき1回だけ（GroundQuery.cpp がメッシュごとに覚えておく）。
    // ・三角形の頂点はメッシュの CPU 側の配列（Mesh::Positions / Indices）を読む。BVH が持つのは箱と三角形の番号だけ。
    class MeshBVH {
    public:
        void Build(const Mesh& mesh);
        bool IsBuiltFor(const Mesh& mesh) const;     // このメッシュ用に作ったか（中身が入れ替わっていないか）

        // メッシュの空間の線 o + t * d で、いちばん小さい t（tMin < t < tMax）の三角形を探す。見つかれば outT に入れて true
        bool RayClosest(const Mesh& mesh, const DirectX::XMFLOAT3& o, const DirectX::XMFLOAT3& d,
            float tMin, float tMax, float& outT) const;
        // メッシュの空間の線 o + t * d と交わる三角形のうち、いちばん大きい t（t <= tMax）を探す（向きは問わない）。
        // 地面の高さ用：ワールドの真上向きの線をメッシュの空間へ移すと、t がそのままワールドの高さになる
        bool LineHighest(const Mesh& mesh, const DirectX::XMFLOAT3& o, const DirectX::XMFLOAT3& d,
            float tMax, float& outT) const;

        size_t NodeCount() const { return nodes.size(); }
        size_t TriangleCount() const { return triIds.size(); }

    private:
        struct Node {
            float    bmin[3];
            float    bmax[3];
            uint32_t start;     // 葉：triIds の最初の位置。枝：右の子の番号（左の子はすぐ次 = 自分 + 1）
            uint32_t count;     // 葉：三角形の数。枝：0
        };
        std::vector<Node>     nodes;
        std::vector<uint32_t> triIds;           // 三角形の番号（葉ごとに並べ替えてある）
        const void* builtPositions = nullptr;   // 作ったときのメッシュの配列（入れ替わったら作り直す印）
        size_t      builtIndexCount = 0;
    };

} // namespace uel
