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
        bool canAct = in.enabled && !(myHp && myHp->hp <= 0.0f);   // 死亡中は操作不可

        // 対象メッシュ取得（assetId空は手続き立方体）
        auto meshOf = [&](Entity* ge) -> std::shared_ptr<Mesh> {
            auto* gm = ge->GetComponent<MeshRendererComponent>();
            if (!gm) return nullptr;
            return gm->assetId.empty() ? ctx.meshCache.GetCube(ctx.device)
                : ctx.meshCache.GetOrLoad(ctx.device, gm->assetId);
            };

        // 見回し
        if (in.enabled && in.lookActive) {
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
        if (in.enabled) {
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
        if (grounded && in.enabled && in.jumpPressed) velY = 6.0f;
        velY -= 15.0f * dt;
        t->position.y += velY * dt;
        if (t->position.y < ground) { t->position.y = ground; velY = 0.0f; }

        // 射撃（前方レイ、当たったEntityを赤く）
        float cp = cosf(pitch), sp = sinf(pitch);
        XMFLOAT3 eyePos = { t->position.x + sy * eyeForward, t->position.y + eyeHeight, t->position.z + cy * eyeForward };
        if (in.enabled && in.shootClicked) {
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
                    if (hpc->hp <= 0.0f)
                        ctx.world.DestroyEntity(hitE->id);             // 撃破→消去
                }
            }
        }

        // カメラ
        XMVECTOR look = XMVectorSet(cp * sy, sp, cp * cy, 0);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        XMVECTOR eye = XMVectorSet(eyePos.x, eyePos.y, eyePos.z, 0);
        XMStoreFloat4x4(&view, XMMatrixLookAtLH(eye, XMVectorAdd(eye, look), up));
    }

} // namespace uel