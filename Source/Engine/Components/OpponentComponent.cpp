#include "Engine/Components/OpponentComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Components/PlayerComponent.h"
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

    } // namespace

    void OpponentComponent::Update(GameContext& ctx)
    {
        Entity& self = ctx.self;
        auto* t = self.GetComponent<TransformComponent>();
        if (!t) return;
        float dt = ctx.dt;
        AnimatorComponent* an = self.GetComponent<AnimatorComponent>();     // 無くても動く（アニメが変わらないだけ）

        // 死亡：HP が 0 になったら、すぐには消さず「倒れている最中」にする
        if (!dying) {
            auto* myHp = self.GetComponent<HPComponent>();
            if (myHp && myHp->hp <= 0.0f) {
                dying = true;
                deathTimer = an ? deathDelay : 0.0f;    // アニメの無い敵は、待たずに消える（今までと同じ）
                hitTimer = -1.0f;                       // 出かかっていた攻撃は取り消す
                path.clear();
            }
        }
        if (dying) {
            SetAnimValue(an, kAnimDead, 1.0f);          // アニメーションへ：倒れた（Bool。消えるまで true のまま）
            deathTimer -= dt;
            // 自分の Entity は、この場では消せない（いま実行中のこの Component ごと無くなってしまう）。
            // 予約だけして、全員の Update が終わった後に World がまとめて消す
            if (deathTimer <= 0.0f) ctx.world.DestroyLater(self.id);
        }

        // プレイヤー探索
        Entity* player = nullptr;
        for (const auto& slot : ctx.world.Slots()) {
            if (!slot) continue;
            if (slot->GetComponent<PlayerComponent>()) { player = slot.get(); break; }
        }

        float distPlayer = 1e9f;
        float movedSpeed = 0.0f;    // このフレームに実際に動いた速さ（止まっていれば 0）
        if (player && !dying) {     // 倒れている最中は、追いかけない・向きも変えない
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
                if (distPlayer > stopRange && l > 1e-4f && moveLockTimer <= 0.0f) {    // 攻撃のあいだ（moveLockTimer）は動かない
                    float step = moveSpeed * dt;
                    t->position.x += dx / l * step;
                    t->position.z += dz / l * step;
                    movedSpeed = moveSpeed;
                }

                // 向き：停止圏内はプレイヤー方向、移動中は進行方向。近すぎる時は更新しない（スピン防止）
                float faceX = (distPlayer <= stopRange) ? dxp : dx;
                float faceZ = (distPlayer <= stopRange) ? dzp : dz;
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
            if (player && distPlayer <= attackRange && attackTimer <= 0.0f) {
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

                if (hitNow && player && distPlayer <= attackRange * 1.25f)    // 少し離れたくらいでは外れない（振っている間に逃げ切れば外れる）
                    if (auto* php = player->GetComponent<HPComponent>()) php->hp -= attackDamage;
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
