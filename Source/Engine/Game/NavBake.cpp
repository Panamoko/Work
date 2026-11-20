#include "Engine/Game/NavBake.h"
#include "Engine/AI/EngineNavGrid.h"
#include "Engine/Scene/World.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Components/NavBoundsComponent.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <vector>

using namespace DirectX;

namespace uel {

    void BakeNavForWorld(EngineNavGrid& nav, World& world, MeshCache& cache, ID3D11Device* device)
    {
        // 範囲の箱（最初に見つかった1つ）
        const NavBoundsComponent* nb = nullptr;
        const TransformComponent* nbT = nullptr;
        const Entity* nbE = nullptr;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            if (const auto* b = slot->GetComponent<NavBoundsComponent>()) {
                nb = b; nbT = slot->GetComponent<TransformComponent>(); nbE = slot.get();
                break;
            }
        }
        if (!nb || !nbT) {                                  // 箱が無い：今までどおり
            nav.Configure({ 0.0f, 0.0f, 0.0f }, 1.0f, 60, 60);
            nav.Bake(world);
            return;
        }
        const float cell = (std::max)(0.1f, nb->cellSize);
        const int cols = (std::max)(1, (std::min)(1024, (int)std::ceil(nb->halfX * 2.0f / cell)));    // 大きすぎる箱は 1024 マスまで
        const int rows = (std::max)(1, (std::min)(1024, (int)std::ceil(nb->halfZ * 2.0f / cell)));
        nav.Configure(nbT->position, cell, cols, rows);

        // 地面になるメッシュを先に集めておく（マスは数万あるので、マスごとに Scene を探したり、Mesh を探し直したりしない）。
        // 箱の Entity 自身のメッシュ（「+ 作成」で付く立方体）は除く
        struct GroundMesh { std::shared_ptr<Mesh> mesh; XMFLOAT4X4 world; const std::vector<uint8_t>* noCol; };
        std::vector<GroundMesh> grounds;
        for (const auto& slot : world.Slots()) {
            if (!slot || slot.get() == nbE) continue;
            const auto* mr = slot->GetComponent<MeshRendererComponent>();
            const auto* t = slot->GetComponent<TransformComponent>();
            if (!mr || !mr->visible || !t) continue;
            auto mp = mr->assetId.empty() ? cache.GetCube(device) : cache.GetOrLoad(device, mr->assetId);
            if (!mp || !mp->IsValid()) continue;
            grounds.push_back({ mp, t->GetWorldMatrix(), &mr->noCollisionSubmesh });
        }

        NavGroundSettings s;
        s.minY = nbT->position.y - nb->halfY;
        s.maxY = nbT->position.y + nb->halfY;
        s.maxStep = nb->maxStep;
        nav.Bake(world, [&grounds](float x, float z, float maxY, float& outY) {
            bool found = false;
            float best = -FLT_MAX;
            for (const GroundMesh& g : grounds) {
                float y;
                if (RaycastGroundColumn(*g.mesh, XMLoadFloat4x4(&g.world), x, z, y, maxY, g.noCol) && y > best) { best = y; found = true; }
            }
            if (found) outY = best;
            return found;
            }, s);
    }

} // namespace uel
