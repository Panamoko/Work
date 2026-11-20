#pragma once
#include <DirectXMath.h>
#include <vector>
#include <cstdint>
#include <functional>

namespace uel {
    class World;

    // 地面を見てグリッドを作るときの決まり（5-4c。NavBoundsComponent の値から作る）
    struct NavGroundSettings {
        float minY = -1e9f;     // 地面がこれより低いマスは歩けない（箱の底。海の底の床など）
        float maxY = 1e9f;      // これより高い面は地面として見ない（箱の上）
        float maxStep = 1.0f;   // 隣のマスとの段差がこれより大きいマスは歩けない（崖）
    };
    // (x, z) の地面の高さを返す関数（maxY より上の面は見ない）。見つからなければ false。
    // グリッドはメッシュや Component を知らないので、地面の調べ方は呼ぶ側（ViewportPanel）が渡す（依存を逆にする）
    using NavGroundFn = std::function<bool(float x, float z, float maxY, float& outY)>;

    // XZ平面のグリッド。NavObstacleComponentからbake→A*で経路探索。
    class EngineNavGrid {
    public:
        void Configure(DirectX::XMFLOAT3 center, float cellSize, int cols, int rows);
        void Bake(World& world);
        // 地面も見て作る（5-4c。UE の NavMesh の自動生成を簡単にしたもの）。NavObstacle で塞いだ後、
        // マスごとに地面の高さを調べ、地面が無い・範囲の外・隣との段差が大きいマスを塞ぐ。経路の点の高さはマスの地面の高さになる
        void Bake(World& world, const NavGroundFn& ground, const NavGroundSettings& settings);
        bool FindPath(DirectX::XMFLOAT3 start, DirectX::XMFLOAT3 goal,
            std::vector<DirectX::XMFLOAT3>& outPath) const;

        int   Cols() const { return cols; }
        int   Rows() const { return rows; }
        float CellSize() const { return cellSize; }
        DirectX::XMFLOAT3 Center() const { return center; }
        bool  Blocked(int cx, int cz) const { return InBounds(cx, cz) && blocked[Idx(cx, cz)] != 0; }
        bool  WorldToCell(DirectX::XMFLOAT3 p, int& cx, int& cz) const;
        DirectX::XMFLOAT3 CellCenterWorld(int cx, int cz) const;

    private:
        DirectX::XMFLOAT3 center{ 0,0,0 };
        float cellSize = 1.0f;
        int cols = 32, rows = 32;
        std::vector<uint8_t> blocked;
        std::vector<float>   heights;   // マスの地面の高さ（地面を見て作ったときだけ。空なら center.y）

        int  Idx(int cx, int cz) const { return cz * cols + cx; }
        bool InBounds(int cx, int cz) const { return cx >= 0 && cx < cols && cz >= 0 && cz < rows; }
        bool NearestWalkable(int& cx, int& cz) const;
    };

} // namespace uel