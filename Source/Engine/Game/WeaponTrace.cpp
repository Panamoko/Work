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
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include "Engine/Physics/HitShapes.h"
#include <memory>

using namespace DirectX;

namespace uel {
    namespace {

        // 弾が素通りする物：倒れている最中の敵と、戦闘不能の味方（倒れているのに、立った形の当たり判定で止まらないように）
        bool IgnoredByShots(const Entity& e)
        {
            if (auto* oc = e.GetComponent<OpponentComponent>()) if (oc->IsDying()) return true;
            if (auto* al = e.GetComponent<AllyComponent>()) if (al->IsIncapacitated()) return true;
            return false;
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
        Entity* meshE = nullptr; float meshT = maxDistance;
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
            if (RaycastMesh(*mp, gw, origin, dir, meshT, tt)) { meshT = tt; meshE = ge; }
        }

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

} // namespace uel
