#pragma once
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

namespace uel {
    class World;

    // XZ平面のグリッド。NavObstacleComponentからbake→A*で経路探索。
    class EngineNavGrid {
    public:
        void Configure(DirectX::XMFLOAT3 center, float cellSize, int cols, int rows);
        void Bake(World& world);
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

        int  Idx(int cx, int cz) const { return cz * cols + cx; }
        bool InBounds(int cx, int cz) const { return cx >= 0 && cx < cols && cz >= 0 && cz < rows; }
        bool NearestWalkable(int& cx, int& cz) const;
    };

} // namespace uel