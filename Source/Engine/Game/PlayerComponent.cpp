#include "Engine/Game/PlayerComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/InputState.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include "Engine/Scene/HPComponent.h"
#include "Engine/Scene/SkinnedMeshComponent.h"
#include "Engine/Game/HitboxComponent.h"
#include "Engine/Physics/HitShapes.h"
#include "Engine/Game/OpponentComponent.h"
#include "Engine/Scene/AnimatorComponent.h"

#include <cmath>
#include <memory>

using namespace DirectX;

namespace uel {

    void PlayerComponent::Update(GameContext& ctx)
    {
        Entity& self = ctx.self;
        auto* t = self.GetComponent<TransformComponent>();
        if (!t) return;
        const InputState& in = ctx.input;
        float dt = ctx.dt;
        if (hitMarker > 0.0f) hitMarker -= dt;
        auto* myHp = self.GetComponent<HPComponent>();
        const bool isDead = (myHp && myHp->hp <= 0.0f);
        const bool canAct = in.enabled && !isDead;   // 死亡中は操作不可

        // 対象メッシュ取得（assetId空は手続き立方体）
        auto meshOf = [&](Entity* ge) -> std::shared_ptr<Mesh> {
            auto* gm = ge->GetComponent<MeshRendererComponent>();
            if (!gm) return nullptr;
            return gm->assetId.empty() ? ctx.meshCache.GetCube(ctx.device)
                : ctx.meshCache.GetOrLoad(ctx.device, gm->assetId);
            };

        // 見回し
        if (canAct && in.lookActive) {
            t->rotationEuler.y += in.mouseDX * 0.15f;
            pitch -= in.mouseDY * 0.004f;
            const float lim = 1.55f;
            if (pitch > lim) pitch = lim;
            if (pitch < -lim) pitch = -lim;
        }

        float yaw = XMConvertToRadians(t->rotationEuler.y);
        float sy = sinf(yaw), cy = cosf(yaw);

        // 移動量
        float dx = 0.0f, dz = 0.0f;
        float speed = moveSpeed * dt;
        if (canAct) {
            if (in.keyW) { dx += sy * speed; dz += cy * speed; }
            if (in.keyS) { dx -= sy * speed; dz -= cy * speed; }
            if (in.keyD) { dx += cy * speed; dz -= sy * speed; }
            if (in.keyA) { dx -= cy * speed; dz += sy * speed; }
        }

        // 壁（水平レイ、self除外、スライド）
        const float bodyH = 0.5f;
        auto wallBlocked = [&](float ox, float oz) -> bool {
            float len = sqrtf(ox * ox + oz * oz);
            if (len < 1e-5f) return false;
            XMFLOAT3 o = { t->position.x, t->position.y + bodyH, t->position.z };
            XMFLOAT3 d = { ox / len, 0.0f, oz / len };
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot) continue; Entity* ge = slot.get();
                if (ge == &self) continue;
                auto* gt = ge->GetComponent<TransformComponent>();
                auto* gm = ge->GetComponent<MeshRendererComponent>();
                if (!gt || !gm || !gm->visible) continue;
                auto mp = meshOf(ge);
                if (!mp || !mp->IsValid()) continue;
                XMFLOAT4X4 gwm = gt->GetWorldMatrix(); XMMATRIX gw = XMLoadFloat4x4(&gwm);
                float hit;
                if (RaycastMesh(*mp, gw, o, d, radius + len, hit)) return true;
            }
            return false;
            };
        const float beforeX = t->position.x, beforeZ = t->position.z;
        if (!wallBlocked(dx, dz)) { t->position.x += dx; t->position.z += dz; }
        else { if (!wallBlocked(dx, 0)) t->position.x += dx; if (!wallBlocked(0, dz)) t->position.z += dz; }

        // 地面高さ
        float ground = groundY;
        for (const auto& slot : ctx.world.Slots()) {
            if (!slot) continue; Entity* ge = slot.get();
            if (ge == &self) continue;
            auto* gt = ge->GetComponent<TransformComponent>();
            auto* gm = ge->GetComponent<MeshRendererComponent>();
            if (!gt || !gm || !gm->visible) continue;
            auto mp = meshOf(ge);
            if (!mp || !mp->IsValid()) continue;
            XMFLOAT4X4 gwm = gt->GetWorldMatrix(); XMMATRIX gw = XMLoadFloat4x4(&gwm);
            float gy;
            if (RaycastGroundColumn(*mp, gw, t->position.x, t->position.z, gy))
                if (gy <= t->position.y + eyeHeight && gy > ground) ground = gy;
        }

        // 重力・接地・ジャンプ
        bool grounded = (t->position.y <= ground + 0.001f);
        const bool jumped = grounded && canAct && in.jumpPressed;
        if (jumped) velY = 6.0f;
        velY -= 15.0f * dt;
        t->position.y += velY * dt;
        if (t->position.y < ground) { t->position.y = ground; velY = 0.0f; }

        // 射撃（前方レイ、当たったEntityを赤く）
        float cp = cosf(pitch), sp = sinf(pitch);
        XMFLOAT3 eyePos = { t->position.x + sy * eyeForward, t->position.y + eyeHeight, t->position.z + cy * eyeForward };
        const bool shot = canAct && in.shootClicked;
        if (shot) {
            XMFLOAT3 rd = { cp * sy, sp, cp * cy };
            const float baseDamage = 34.0f;

            // (1) ヒットボックス（胴体カプセル＋頭の球）
            Entity* hbE = nullptr; float hbT = 1000.0f; bool head = false;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot) continue; Entity* ge = slot.get();
                if (ge == &self) continue;
                auto* gt = ge->GetComponent<TransformComponent>();
                auto* hb = ge->GetComponent<HitboxComponent>();
                if (!gt || !hb) continue;
                if (auto* oc = ge->GetComponent<OpponentComponent>()) if (oc->IsDying()) continue;   // 倒れている最中の敵には当たらない
                auto ws = hb->GetWorldShapes(*gt);
                float tt;
                if (RaySphere(eyePos, rd, ws.headC, ws.headR, tt) && tt < hbT) { hbT = tt; hbE = ge; head = true; }
                if (RayCapsule(eyePos, rd, ws.bodyA, ws.bodyB, ws.bodyR, tt) && tt < hbT) { hbT = tt; hbE = ge; head = false; }
            }

            // (2) メッシュ（壁・床などの遮蔽物。ヒットボックスを持つものはヒットボックスで判定済みなので除外）
            Entity* meshE = nullptr; float meshT = 1000.0f;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot) continue; Entity* ge = slot.get();
                if (ge == &self) continue;
                if (ge->GetComponent<HitboxComponent>()) continue;
                if (auto* oc = ge->GetComponent<OpponentComponent>()) if (oc->IsDying()) continue;   // 倒れている最中の敵には当たらない
                auto* gt = ge->GetComponent<TransformComponent>();
                auto* gm = ge->GetComponent<MeshRendererComponent>();
                bool skinned = (ge->GetComponent<SkinnedMeshComponent>() != nullptr);
                if (!gt || !gm) continue;
                if (!gm->visible && !skinned) continue;
                auto mp = meshOf(ge);
                if (!mp || !mp->IsValid()) continue;
                XMFLOAT4X4 gwm = gt->GetWorldMatrix(); XMMATRIX gw = XMLoadFloat4x4(&gwm);
                float tt;
                if (RaycastMesh(*mp, gw, eyePos, rd, meshT, tt)) { meshT = tt; meshE = ge; }
            }

            // (3) 近いほうに当たる（壁の向こうのヒットボックスには当たらない）
            Entity* hitE = nullptr; float mul = 1.0f; bool headHit = false;
            if (hbE && hbT <= meshT) {
                hitE = hbE; headHit = head;
                if (head) mul = hbE->GetComponent<HitboxComponent>()->headMultiplier;
            }
            else if (meshE) hitE = meshE;

            if (hitE) {
                if (auto* hpc = hitE->GetComponent<HPComponent>()) {
                    hitMarker = 0.15f;
                    lastHeadshot = headHit;
                    hpc->hp -= baseDamage * mul;                        // ダメージ（頭は倍率）
                    if (auto* hm = hitE->GetComponent<MeshRendererComponent>())
                        hm->color = { 1.0f, 0.3f, 0.3f, 1.0f };        // 被弾表示（赤み）
                    if (auto* sk = hitE->GetComponent<SkinnedMeshComponent>())
                        sk->color = { 1.0f, 0.3f, 0.3f, 1.0f };
                    // 撃破：敵（OpponentComponent）は、自分で死亡アニメを再生してから消える。それ以外は、その場で消す
                    if (hpc->hp <= 0.0f && !hitE->GetComponent<OpponentComponent>())
                        ctx.world.DestroyEntity(hitE->id);
                }
            }
        }

        // ---- アニメーションへ：いまの状態を渡す（グラフに同じ名前のパラメータがあるときだけ書く）----
        if (AnimatorComponent* an = self.GetComponent<AnimatorComponent>()) {
            // 実際に動いた量（壁に止められたぶんは含まない）から、速さと向きを求める
            const float mx = t->position.x - beforeX, mz = t->position.z - beforeZ;
            const float inv = (dt > 1e-6f) ? 1.0f / dt : 0.0f;
            const float moved = sqrtf(mx * mx + mz * mz) * inv;                     // m/s
            // 体の向きから見た方向（右が +X、前が +Y）。全速で 1 になるように moveSpeed で割る
            const float norm = (moveSpeed > 1e-4f) ? inv / moveSpeed : 0.0f;
            float localX = (mx * cy - mz * sy) * norm;
            float localY = (mx * sy + mz * cy) * norm;
            if (localX > 1.0f) localX = 1.0f; if (localX < -1.0f) localX = -1.0f;
            if (localY > 1.0f) localY = 1.0f; if (localY < -1.0f) localY = -1.0f;

            an->params.SetIfExists(kAnimSpeed, moved);
            an->params.SetIfExists(kAnimMoveX, localX);
            an->params.SetIfExists(kAnimMoveY, localY);
            an->params.SetIfExists(kAnimGrounded, (t->position.y <= ground + 0.001f) ? 1.0f : 0.0f);
            an->params.SetIfExists(kAnimDead, isDead ? 1.0f : 0.0f);
            if (jumped) an->params.SetIfExists(kAnimJump, 1.0f);                    // Trigger：跳んだ
            if (shot)   an->params.SetIfExists(kAnimAttack, 1.0f);                  // Trigger：撃った
        }

        // カメラ
        XMVECTOR look = XMVectorSet(cp * sy, sp, cp * cy, 0);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        XMVECTOR eye = XMVectorSet(eyePos.x, eyePos.y, eyePos.z, 0);
        XMStoreFloat4x4(&view, XMMatrixLookAtLH(eye, XMVectorAdd(eye, look), up));
    }

} // namespace uel