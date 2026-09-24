#include "Engine/Game/OpponentComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/PlayerComponent.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include <cmath>
#include <memory>

using namespace DirectX;

namespace uel {

    void OpponentComponent::Update(GameContext& ctx)
    {
        Entity& self = ctx.self;
        auto* t = self.GetComponent<TransformComponent>();
        if (!t) return;
        float dt = ctx.dt;

        // プレイヤー（PlayerComponent を持つEntity）を探す
        Entity* player = nullptr;
        for (const auto& slot : ctx.world.Slots()) {
            if (!slot) continue;
            if (slot->GetComponent<PlayerComponent>()) { player = slot.get(); break; }
        }

        // プレイヤーへ直進＋向く
        if (player) {
            if (auto* pt = player->GetComponent<TransformComponent>()) {
                float dxp = pt->position.x - t->position.x;
                float dzp = pt->position.z - t->position.z;
                float len = sqrtf(dxp * dxp + dzp * dzp);
                if (len > stopRange) {
                    float step = moveSpeed * dt;
                    t->position.x += dxp / len * step;
                    t->position.z += dzp / len * step;
                }
                if (len > 1e-4f)
                    t->rotationEuler.y = atan2f(dxp, dzp) * 57.2958f;   // プレイヤーの方を向く
            }
        }

        // 地面追従（自分以外のメッシュ）
        float ground = -1e9f;
        for (const auto& slot : ctx.world.Slots()) {
            if (!slot) continue;
            Entity* ge = slot.get();
            if (ge == &self) continue;
            auto* gt = ge->GetComponent<TransformComponent>();
            auto* gm = ge->GetComponent<MeshRendererComponent>();
            if (!gt || !gm || !gm->visible) continue;
            auto mp = gm->assetId.empty() ? ctx.meshCache.GetCube(ctx.device)
                : ctx.meshCache.GetOrLoad(ctx.device, gm->assetId);
            if (!mp || !mp->IsValid()) continue;
            XMFLOAT4X4 gwm = gt->GetWorldMatrix(); XMMATRIX gw = XMLoadFloat4x4(&gwm);
            float gy;
            if (RaycastGroundColumn(*mp, gw, t->position.x, t->position.z, gy))
                if (gy <= t->position.y + 1.5f && gy > ground) ground = gy;
        }

        // 重力＆接地
        velY -= 15.0f * dt;
        float ny = t->position.y + velY * dt;
        if (ground > -1e8f && ny < ground) { ny = ground; velY = 0.0f; }
        t->position.y = ny;
    }

} // namespace uel