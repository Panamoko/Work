#include "Engine/Game/WeaponTrace.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/Team.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Components/SkinnedMeshComponent.h"
#include "Engine/Components/HitboxComponent.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Components/OpponentComponent.h"
#include "Engine/Components/AllyComponent.h"
#include "Engine/Components/LifeStateComponent.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include "Engine/Physics/HitShapes.h"
#include <cmath>
#include <memory>

using namespace DirectX;

namespace uel {
    namespace {

        // 弾が素通りする物：倒れている最中の敵と、ダウン・Dead・戦闘不能の味方（倒れているのに、立った形の当たり判定で止まらないように）
        bool IgnoredByShots(const Entity& e)
        {
            if (auto* oc = e.GetComponent<OpponentComponent>()) if (oc->IsDying()) return true;
            if (auto* ls = e.GetComponent<LifeStateComponent>()) if (!ls->IsAlive()) return true;     // プレイヤーも AI 仲間も（段階 4）
            if (auto* al = e.GetComponent<AllyComponent>()) if (al->IsIncapacitated()) return true;    // 状態を持たない AI 仲間（HP 0）
            return false;
        }

        // メッシュ（壁・床などの遮蔽物）への線。ヒットボックスを持つ物はヒットボックスで調べるので除く。
        // いちばん手前の物と距離を返す（TraceShot と IsLineBlocked で共用）
        Entity* TraceMeshes(GameContext& ctx, const XMFLOAT3& origin, const XMFLOAT3& dir, float maxDistance, float& outT)
        {
            const Entity& self = ctx.self;
            Entity* meshE = nullptr;
            float meshT = maxDistance;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot) continue; Entity* ge = slot.get();
                if (ge == &self || IgnoredByShots(*ge)) continue;
                if (ge->GetComponent<HitboxComponent>()) continue;
                auto* gt = ge->GetComponent<TransformComponent>();
                auto* gm = ge->GetComponent<MeshRendererComponent>();
                const bool skinned = (ge->GetComponent<SkinnedMeshComponent>() != nullptr);
                if (!gt || !gm) continue;
                if (!gm->visible && !skinned) continue;     // 見えないメッシュは、スキンドのキャラの体だけ調べる
                auto mp = gm->assetId.empty() ? ctx.meshCache.GetCube(ctx.device)
                    : ctx.meshCache.GetOrLoad(ctx.device, gm->assetId);
                if (!mp || !mp->IsValid()) continue;
                XMFLOAT4X4 gwm = gt->GetWorldMatrix(); XMMATRIX gw = XMLoadFloat4x4(&gwm);
                float tt;
                if (RaycastMesh(*mp, gw, origin, dir, meshT, tt, &gm->noCollisionSubmesh)) { meshT = tt; meshE = ge; }   // 当たり判定なしのサブメッシュは素通り（5-4b）
            }
            outT = meshT;
            return meshE;
        }

    } // namespace

    ShotHit TraceShot(GameContext& ctx, const XMFLOAT3& origin, const XMFLOAT3& dir, float maxDistance)
    {
        const Entity& self = ctx.self;

        // (1) ヒットボックス（胴体カプセル＋頭の球）
        Entity* hbE = nullptr; float hbT = maxDistance; bool head = false;
        for (const auto& slot : ctx.world.Slots()) {
            if (!slot) continue; Entity* ge = slot.get();
            if (ge == &self || IgnoredByShots(*ge)) continue;
            auto* gt = ge->GetComponent<TransformComponent>();
            auto* hb = ge->GetComponent<HitboxComponent>();
            if (!gt || !hb) continue;
            auto ws = hb->GetWorldShapes(*gt);
            float tt;
            if (RaySphere(origin, dir, ws.headC, ws.headR, tt) && tt < hbT) { hbT = tt; hbE = ge; head = true; }
            if (RayCapsule(origin, dir, ws.bodyA, ws.bodyB, ws.bodyR, tt) && tt < hbT) { hbT = tt; hbE = ge; head = false; }
        }

        // (2) メッシュ（壁・床などの遮蔽物。ヒットボックスを持つ物はヒットボックスで調べたので除く）
        float meshT = maxDistance;
        Entity* meshE = TraceMeshes(ctx, origin, dir, maxDistance, meshT);

        // (3) 近いほうに当たる（壁の向こうのヒットボックスには当たらない）
        ShotHit hit;
        if (hbE && hbT <= meshT) {
            hit.entity = hbE; hit.distance = hbT; hit.headshot = head;
            if (head) hit.damageMultiplier = hbE->GetComponent<HitboxComponent>()->headMultiplier;
        }
        else if (meshE) {
            hit.entity = meshE; hit.distance = meshT;
        }
        return hit;
    }

    bool ApplyShotDamage(GameContext& ctx, const ShotHit& hit, float baseDamage)
    {
        Entity* target = hit.entity;
        if (!target) return false;
        if (IsFriendly(ctx.self, *target)) return false;   // 味方に当たった弾は、そこで止まる（ダメージ無し）
        auto* hpc = target->GetComponent<HPComponent>();
        if (!hpc) return false;                             // 壁など

        hpc->hp -= baseDamage * hit.damageMultiplier;       // ダメージ（頭は倍率）
        if (auto* hm = target->GetComponent<MeshRendererComponent>())
            hm->color = { 1.0f, 0.3f, 0.3f, 1.0f };         // 被弾表示（赤み）
        if (auto* sk = target->GetComponent<SkinnedMeshComponent>())
            sk->color = { 1.0f, 0.3f, 0.3f, 1.0f };
        // 撃破：陣営の無い物（的・箱など）だけ、その場で消す。
        // 敵（Enemies）は自分で死亡アニメを再生してから消える。味方（Players）は消さない（戦闘不能・ダウンは自分で扱う）
        if (hpc->hp <= 0.0f && GetTeam(*target) == Team::None)
            ctx.world.DestroyEntity(target->id);
        return true;
    }

    bool IsLineBlocked(GameContext& ctx, const XMFLOAT3& from, const XMFLOAT3& to)
    {
        const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
        const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1e-4f) return false;
        const XMFLOAT3 dir = { dx / len, dy / len, dz / len };
        float t = len;
        return TraceMeshes(ctx, from, dir, len, t) != nullptr;     // to より手前で、何かのメッシュに当たった
    }

} // namespace uel
