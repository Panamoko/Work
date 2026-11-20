#include "Engine/Components/OpponentComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/Team.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include "Engine/AI/EngineNavGrid.h"
#include <cmath>
#include <memory>

using namespace DirectX;

namespace uel {
    namespace {

        // グラフに、その名前のパラメータがあるときだけ値を書く。
        // （無いときに作ってしまうと、関係ないグラフにパラメータが増えていくので、作らない）
        void SetAnimValue(AnimatorComponent* an, const char* name, float value)
        {
            if (!an) return;
            const int i = an->params.Find(name);
            if (i >= 0) an->params.items[i].value = value;
        }

        // 狙える相手か：敵対する陣営（Players）で、HP が残っている。
        // HP を持たない物は倒せないので狙わない（AI 仲間に HPComponent を付け忘れると、狙われない）
        bool IsTargetable(const Entity& self, const Entity& e)
        {
            if (!IsHostile(self, e)) return false;
            auto* hp = e.GetComponent<HPComponent>();
            return hp && hp->hp > 0.0f;
        }

        // XZ 平面での距離（高さの違いは見ない）
        float DistXZ(const XMFLOAT3& a, const XMFLOAT3& b)
        {
            const float dx = a.x - b.x, dz = a.z - b.z;
            return sqrtf(dx * dx + dz * dz);
        }

    } // namespace

    void OpponentComponent::Update(GameContext& ctx)
    {
        Entity& self = ctx.self;
        auto* t = self.GetComponent<TransformComponent>();
        if (!t) return;
        float dt = ctx.dt;
        AnimatorComponent* an = self.GetComponent<AnimatorComponent>();     // 無くても動く（アニメが変わらないだけ）

        HPComponent* myHp = self.GetComponent<HPComponent>();

        // 死亡：HP が 0 になったら、すぐには消さず「倒れている最中」にする
        if (!dying) {
            if (myHp && myHp->hp <= 0.0f) {
                dying = true;
                deathTimer = an ? deathDelay : 0.0f;    // アニメの無い敵は、待たずに消える（今までと同じ）
                hitTimer = -1.0f;                       // 出かかっていた攻撃は取り消す
                path.clear();
                if (an) an->StopMontage(0.1f);          // ひるみの途中でも、すぐ倒れる動きへ移る
            }
        }

        // ひるみ：HP が減っていたら（撃たれた）、モンタージュ "HitReact" を頭から再生する。
        // ひるんでいる間は動かず、出かかっていた攻撃も取り消す（ひるみの最中に殴られないように）
        if (hitReactTimer > 0.0f) hitReactTimer -= dt;
        if (!dying && myHp && lastHp >= 0.0f && myHp->hp < lastHp) {
            if (an && an->PlayMontage(kMontageHitReact)) {     // モンタージュが無ければ、ひるまない
                hitReactTimer = hitReactTime;
                hitTimer = -1.0f;
            }
        }
        if (myHp) lastHp = myHp->hp;
        if (dying) {
            SetAnimValue(an, kAnimDead, 1.0f);          // アニメーションへ：倒れた（Bool。消えるまで true のまま）
            deathTimer -= dt;
            // 自分の Entity は、この場では消せない（いま実行中のこの Component ごと無くなってしまう）。
            // 予約だけして、全員の Update が終わった後に World がまとめて消す
            if (deathTimer <= 0.0f) ctx.world.DestroyLater(self.id);
        }

        // 狙う相手：敵対する陣営（プレイヤー・AI 仲間）の中で、HP が残っていていちばん近い相手。
        // 毎フレーム World 全体を探さず、retargetInterval ごとに選び直す（今の相手が消えた・HP が 0 になったときは、すぐ選び直す）。
        // 今の相手より retargetMargin 以上近い相手がいるときだけ乗り換える（同じくらいの距離の2人の間で、狙いが行ったり来たりしないように）
        Entity* target = ctx.world.Get(targetId);
        if (targetId.IsValid() && (!target || !IsTargetable(self, *target))) {
            target = nullptr;
            targetId = EntityId{};
            retargetTimer = 0.0f;       // 今の相手がいなくなった：すぐ選び直す
        }
        retargetTimer -= dt;
        if (!dying && retargetTimer <= 0.0f) {      // 相手がいないときも、探すのは retargetInterval ごと（毎フレームは探さない）
            retargetTimer = retargetInterval;
            auto* curT = target ? target->GetComponent<TransformComponent>() : nullptr;
            const float curDist = curT ? DistXZ(curT->position, t->position) : 1e9f;
            Entity* best = nullptr;
            float bestDist = 1e9f;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot || slot.get() == &self || !IsTargetable(self, *slot)) continue;
                auto* ct = slot->GetComponent<TransformComponent>();
                if (!ct) continue;
                const float d = DistXZ(ct->position, t->position);
                if (d < bestDist) { bestDist = d; best = slot.get(); }
            }
            if (best && best != target && (!target || bestDist < curDist - retargetMargin)) {
                target = best;
                repathTimer = 0.0f;     // 相手が変わったら、すぐ経路を作り直す
            }
            targetId = target ? target->id : EntityId{};
        }

        float distTarget = 1e9f;
        float movedSpeed = 0.0f;    // このフレームに実際に動いた速さ（止まっていれば 0）
        if (target && !dying) {     // 倒れている最中は、追いかけない・向きも変えない
            if (auto* pt = target->GetComponent<TransformComponent>()) {
                float dxp = pt->position.x - t->position.x;
                float dzp = pt->position.z - t->position.z;
                distTarget = sqrtf(dxp * dxp + dzp * dzp);

                // 経路再計算（0.25秒毎）
                repathTimer -= dt;
                if (ctx.nav && repathTimer <= 0.0f) {
                    ctx.nav->FindPath(t->position, pt->position, path);
                    repathTimer = 0.25f;
                    wpIndex = (path.size() > 1) ? 1 : 0;   // 開始セルはスキップ
                }

                // 目標ウェイポイント（前進のみ：到達したら次へ進める）
                XMFLOAT3 goal = pt->position;
                if (ctx.nav && !path.empty()) {
                    if (wpIndex >= path.size()) wpIndex = path.size() - 1;
                    float wx = path[wpIndex].x - t->position.x;
                    float wz = path[wpIndex].z - t->position.z;
                    if (sqrtf(wx * wx + wz * wz) < 0.5f && wpIndex + 1 < path.size()) ++wpIndex;
                    goal = path[wpIndex];
                }

                float dx = goal.x - t->position.x;
                float dz = goal.z - t->position.z;
                float l = sqrtf(dx * dx + dz * dz);
                if (distTarget > stopRange && l > 1e-4f && moveLockTimer <= 0.0f && hitReactTimer <= 0.0f) {    // 攻撃のあいだ（moveLockTimer）とひるんでいる間は動かない
                    float step = moveSpeed * dt;
                    t->position.x += dx / l * step;
                    t->position.z += dz / l * step;
                    movedSpeed = moveSpeed;
                }

                // 向き：停止圏内は相手の方向、移動中は進行方向。近すぎる時は更新しない（スピン防止）
                float faceX = (distTarget <= stopRange) ? dxp : dx;
                float faceZ = (distTarget <= stopRange) ? dzp : dz;
                if (sqrtf(faceX * faceX + faceZ * faceZ) > 0.15f)
                    t->rotationEuler.y = atan2f(faceX, faceZ) * 57.2958f;
            }
        }

        // アニメーションへ：いまの移動の速さ
        SetAnimValue(an, kAnimSpeed, movedSpeed);

        if (!dying) {
            // 接触攻撃：届く距離にいて、前の攻撃から時間がたっていれば、攻撃を始める
            attackTimer -= dt;
            moveLockTimer -= dt;
            if (target && distTarget <= attackRange && attackTimer <= 0.0f && hitReactTimer <= 0.0f) {     // ひるんでいる間は攻撃しない
                attackTimer = attackInterval;
                moveLockTimer = attackMoveLock;             // 攻撃のあいだは、その場に止まる
                SetAnimValue(an, kAnimAttack, 1.0f);        // アニメーションへ：攻撃を始めた（Trigger）

                // ダメージを出す瞬間の決め方：
                //   グラフに "Hit" の通知があれば、その通知を待つ（クリップの当たる瞬間に合う）
                //   無ければ、今までどおり attackHitDelay 秒後に出す
                hitByNotify = an && an->graph && an->graph->HasNotify(kNotifyHit);
                hitTimer = hitByNotify ? attackInterval : attackHitDelay;     // 通知を待つ場合は、次の攻撃までに来なければあきらめる
            }

            // 攻撃のダメージ
            if (hitTimer >= 0.0f) {
                bool hitNow = false;
                hitTimer -= dt;
                if (hitByNotify) {
                    // 通知は、前のフレームのアニメの計算で発火したもの（アニメの計算は、ゲームの更新の後にある）
                    if (an && an->WasFired(kNotifyHit)) { hitNow = true; hitTimer = -1.0f; }
                    else if (hitTimer <= 0.0f) hitTimer = -1.0f;         // 通知が来なかった（攻撃の状態に入れなかった、など）：当てずに終える
                }
                else if (hitTimer <= 0.0f) { hitNow = true; hitTimer = -1.0f; }

                if (hitNow && target && distTarget <= attackRange * 1.25f)    // 少し離れたくらいでは外れない（振っている間に逃げ切れば外れる）
                    if (auto* thp = target->GetComponent<HPComponent>()) thp->hp -= attackDamage;
            }
        }

        // 地面追従（倒れている最中も、地面には乗せておく）
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
            if (RaycastGroundColumn(*mp, gw, t->position.x, t->position.z, gy, t->position.y + 1.5f, &gm->noCollisionSubmesh))   // 足元より 1.5m 上まで・当たり判定なしは見ない（5-4b）
                if (gy > ground) ground = gy;
        }

        // 重力→着地
        velY -= 15.0f * dt;
        float ny = t->position.y + velY * dt;
        if (ground > -1e8f && ny < ground) { ny = ground; velY = 0.0f; }
        t->position.y = ny;
    }

} // namespace uel
