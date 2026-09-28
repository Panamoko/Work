#include "Engine/Game/OpponentComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/PlayerComponent.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Scene/HPComponent.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include "Engine/AI/EngineNavGrid.h"
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

        // プレイヤー探索
        Entity* player = nullptr;
        for (const auto& slot : ctx.world.Slots()) {
            if (!slot) continue;
            if (slot->GetComponent<PlayerComponent>()) { player = slot.get(); break; }
        }

        float distPlayer = 1e9f;
        if (player) {
            if (auto* pt = player->GetComponent<TransformComponent>()) {
                float dxp = pt->position.x - t->position.x;
                float dzp = pt->position.z - t->position.z;
                distPlayer = sqrtf(dxp * dxp + dzp * dzp);

                // 経路再計算（0.25秒毎）
                repathTimer -= dt;
                if (ctx.nav && repathTimer <= 0.0f) {
                    ctx.nav->FindPath(t->position, pt->position, path);
                    repathTimer = 0.25f;
                    wpIndex = (path.size() > 1) ? 1 : 0;   // 開始セルはスキップ
                }

                // 目標ウェイポイント（前進のみ：到達したら次へ進める）
                XMFLOAT3 target = pt->position;
                if (ctx.nav && !path.empty()) {
                    if (wpIndex >= path.size()) wpIndex = path.size() - 1;
                    float wx = path[wpIndex].x - t->position.x;
                    float wz = path[wpIndex].z - t->position.z;
                    if (sqrtf(wx * wx + wz * wz) < 0.5f && wpIndex + 1 < path.size()) ++wpIndex;
                    target = path[wpIndex];
                }

                float dx = target.x - t->position.x;
                float dz = target.z - t->position.z;
                float l = sqrtf(dx * dx + dz * dz);
                if (distPlayer > stopRange && l > 1e-4f) {
                    float step = moveSpeed * dt;
                    t->position.x += dx / l * step;
                    t->position.z += dz / l * step;
                }

                // 向き：停止圏内はプレイヤー方向、移動中は進行方向。近すぎる時は更新しない（スピン防止）
                float faceX = (distPlayer <= stopRange) ? dxp : dx;
                float faceZ = (distPlayer <= stopRange) ? dzp : dz;
                if (sqrtf(faceX * faceX + faceZ * faceZ) > 0.15f)
                    t->rotationEuler.y = atan2f(faceX, faceZ) * 57.2958f;
            }
        }

        // 接触攻撃
        attackTimer -= dt;
        if (player && distPlayer <= attackRange && attackTimer <= 0.0f) {
            if (auto* php = player->GetComponent<HPComponent>()) php->hp -= attackDamage;
            attackTimer = attackInterval;
        }

        // 地面追従
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

        // 重力→着地
        velY -= 15.0f * dt;
        float ny = t->position.y + velY * dt;
        if (ground > -1e8f && ny < ground) { ny = ground; velY = 0.0f; }
        t->position.y = ny;
    }

} // namespace uel