#include "Engine/AI/EngineNavGrid.h"
#include "Engine/Components/NavObstacleComponent.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include <queue>
#include <cfloat>
#include <cmath>
#include <algorithm>

using namespace DirectX;

namespace uel {

    void EngineNavGrid::Configure(XMFLOAT3 c, float cs, int co, int ro)
    {
        center = c; cellSize = (cs > 0.01f ? cs : 0.01f);
        cols = std::max(1, co); rows = std::max(1, ro);
        blocked.assign((size_t)cols * rows, 0);
    }

    bool EngineNavGrid::WorldToCell(XMFLOAT3 p, int& cx, int& cz) const
    {
        float ox = center.x - cols * cellSize * 0.5f;
        float oz = center.z - rows * cellSize * 0.5f;
        cx = (int)std::floor((p.x - ox) / cellSize);
        cz = (int)std::floor((p.z - oz) / cellSize);
        return InBounds(cx, cz);
    }

    XMFLOAT3 EngineNavGrid::CellCenterWorld(int cx, int cz) const
    {
        float ox = center.x - cols * cellSize * 0.5f;
        float oz = center.z - rows * cellSize * 0.5f;
        return { ox + (cx + 0.5f) * cellSize, center.y, oz + (cz + 0.5f) * cellSize };
    }

    void EngineNavGrid::Bake(World& world)
    {
        blocked.assign((size_t)cols * rows, 0);
        float ox = center.x - cols * cellSize * 0.5f;
        float oz = center.z - rows * cellSize * 0.5f;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            auto* t = slot->GetComponent<TransformComponent>();
            auto* ob = slot->GetComponent<NavObstacleComponent>();
            if (!t || !ob) continue;
            float px = t->position.x, pz = t->position.z;
            int minx = (int)std::floor((px - ob->halfX - ox) / cellSize);
            int maxx = (int)std::floor((px + ob->halfX - ox) / cellSize);
            int minz = (int)std::floor((pz - ob->halfZ - oz) / cellSize);
            int maxz = (int)std::floor((pz + ob->halfZ - oz) / cellSize);
            for (int z = std::max(0, minz); z <= std::min(rows - 1, maxz); ++z)
                for (int x = std::max(0, minx); x <= std::min(cols - 1, maxx); ++x)
                    blocked[Idx(x, z)] = 1;
        }
    }

    bool EngineNavGrid::NearestWalkable(int& cx, int& cz) const
    {
        if (InBounds(cx, cz) && !blocked[Idx(cx, cz)]) return true;
        int maxR = std::max(cols, rows);
        for (int r = 1; r <= maxR; ++r) {
            for (int dz = -r; dz <= r; ++dz)
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::abs(dx) != r && std::abs(dz) != r) continue; // ÉäÉìÉOäOé¸ÇÃÇ›
                    int nx = cx + dx, nz = cz + dz;
                    if (InBounds(nx, nz) && !blocked[Idx(nx, nz)]) { cx = nx; cz = nz; return true; }
                }
        }
        return false;
    }

    bool EngineNavGrid::FindPath(XMFLOAT3 startW, XMFLOAT3 goalW, std::vector<XMFLOAT3>& outPath) const
    {
        outPath.clear();
        if (blocked.empty()) return false;
        int sx, sz, gx, gz;
        WorldToCell(startW, sx, sz); WorldToCell(goalW, gx, gz);
        sx = std::clamp(sx, 0, cols - 1); sz = std::clamp(sz, 0, rows - 1);
        gx = std::clamp(gx, 0, cols - 1); gz = std::clamp(gz, 0, rows - 1);
        if (!NearestWalkable(sx, sz)) return false;
        if (!NearestWalkable(gx, gz)) return false;

        const int N = cols * rows;
        int start = Idx(sx, sz), goal = Idx(gx, gz);
        if (start == goal) { outPath.push_back(CellCenterWorld(gx, gz)); return true; }

        std::vector<float> g(N, FLT_MAX);
        std::vector<int>   from(N, -1);
        std::vector<uint8_t> closed(N, 0);
        auto H = [&](int x, int z) { int dx = std::abs(x - gx), dz = std::abs(z - gz); int mn = std::min(dx, dz), mx = std::max(dx, dz); return (mx - mn) + 1.41421356f * mn; };

        struct Node { float f; int idx; bool operator>(const Node& o) const { return f > o.f; } };
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
        g[start] = 0.0f; open.push({ H(sx, sz), start });

        const int dxs[8] = { 1,-1,0,0, 1,1,-1,-1 };
        const int dzs[8] = { 0,0,1,-1, 1,-1,1,-1 };

        while (!open.empty()) {
            Node cur = open.top(); open.pop();
            if (cur.idx == goal) break;
            if (closed[cur.idx]) continue;
            closed[cur.idx] = 1;
            int ccx = cur.idx % cols, ccz = cur.idx / cols;
            for (int d = 0; d < 8; ++d) {
                int nx = ccx + dxs[d], nz = ccz + dzs[d];
                if (!InBounds(nx, nz) || blocked[Idx(nx, nz)]) continue;
                if (d >= 4) { // éŒÇﬂÅFäpî≤ÇØñhé~
                    if (blocked[Idx(ccx + dxs[d], ccz)] || blocked[Idx(ccx, ccz + dzs[d])]) continue;
                }
                float step = (d < 4) ? 1.0f : 1.41421356f;
                int ni = Idx(nx, nz);
                float ng = g[cur.idx] + step;
                if (ng < g[ni]) { g[ni] = ng; from[ni] = cur.idx; open.push({ ng + H(nx, nz), ni }); }
            }
        }

        if (from[goal] < 0 && goal != start) return false;
        std::vector<XMFLOAT3> rev;
        for (int c = goal; c != -1; c = from[c]) rev.push_back(CellCenterWorld(c % cols, c / cols));
        outPath.assign(rev.rbegin(), rev.rend());
        return !outPath.empty();
    }

} // namespace uel