#include "Engine/Components/AllyComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Components/PlayerComponent.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include "Engine/AI/EngineNavGrid.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace uel {
    namespace {

        // XZ 平面での距離（高さの違いは見ない）
        float DistXZ(const XMFLOAT3& a, const XMFLOAT3& b)
        {
            const float dx = a.x - b.x, dz = a.z - b.z;
            return std::sqrt(dx * dx + dz * dz);
        }

        // 足元の地面の高さ。OpponentComponent と同じ調べ方（表示中の MeshRenderer のメッシュを真下に調べる）。
        // 今の位置より 1.5m 以上高い面は、頭の上の物とみなして無視する。
        bool FindGround(GameContext& ctx, const Entity& self, const XMFLOAT3& p, float& outY)
        {
            bool  found = false;
            float best = -1e9f;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot || slot.get() == &self) continue;
                auto* gm = slot->GetComponent<MeshRendererComponent>();
                if (!gm || !gm->visible) continue;
                auto* gt = slot->GetComponent<TransformComponent>();
                if (!gt) continue;
                auto mp = gm->assetId.empty() ? ctx.meshCache.GetCube(ctx.device)
                    : ctx.meshCache.GetOrLoad(ctx.device, gm->assetId);
                if (!mp || !mp->IsValid()) continue;
                XMFLOAT4X4 wm = gt->GetWorldMatrix();
                float y;
                if (RaycastGroundColumn(*mp, XMLoadFloat4x4(&wm), p.x, p.z, y) && y <= p.y + 1.5f && y > best) {
                    best = y;
                    found = true;
                }
            }
            if (found) outY = best;
            return found;
        }

        // グラフに、その名前のパラメータがあるときだけ値を書く（無いときに作らない）
        void SetAnimValue(AnimatorComponent* an, const char* name, float value)
        {
            if (!an) return;
            an->params.SetIfExists(name, value);
        }

    } // namespace

    void AllyComponent::Update(GameContext& ctx)
    {
        Entity& self = ctx.self;
        auto* t = self.GetComponent<TransformComponent>();
        if (!t) return;
        const float dt = ctx.dt;

        // 1) 追いかける相手。覚えている相手がいなくなったときだけ World を探し直す（毎フレーム全員を調べない）
        Entity* lead = ctx.world.Get(leader);
        if (!lead) {
            for (const auto& slot : ctx.world.Slots()) {
                if (slot && slot.get() != &self && slot->GetComponent<PlayerComponent>()) {
                    lead = slot.get();
                    leader = lead->id;
                    break;
                }
            }
        }
        TransformComponent* lt = lead ? lead->GetComponent<TransformComponent>() : nullptr;

        const XMFLOAT3 before = t->position;
        float movedSpeed = 0.0f;
        if (lt) {
            // 2) 動くか止まるか。止まる距離と動き出す距離を分けて、境目で動いたり止まったりを繰り返さないようにする
            const float dist = DistXZ(t->position, lt->position);
            if (following && dist <= followDistance) { following = false; path.clear(); }
            else if (!following && dist > resumeDistance) { following = true; repathTimer = 0.0f; }

            if (following) {
                // 3) 経路：一定の間隔で、しかも相手が少し動いていたときだけ計算し直す（A* を毎フレーム回さない）
                repathTimer -= dt;
                if (ctx.nav && repathTimer <= 0.0f) {
                    repathTimer = repathInterval;
                    if (path.empty() || DistXZ(pathGoal, lt->position) > 0.5f) {
                        ctx.nav->FindPath(t->position, lt->position, path);
                        pathGoal = lt->position;
                        wpIndex = (path.size() > 1) ? 1 : 0;    // 最初の点は自分のいるマスなので飛ばす
                    }
                }

                // 次に向かう点。経路が無い（グリッドの外など）ときは、相手へまっすぐ向かう
                XMFLOAT3 target = lt->position;
                if (!path.empty()) {
                    if (wpIndex >= path.size()) wpIndex = path.size() - 1;
                    if (DistXZ(path[wpIndex], t->position) < 0.5f && wpIndex + 1 < path.size()) ++wpIndex;
                    target = path[wpIndex];
                }

                const float dx = target.x - t->position.x;
                const float dz = target.z - t->position.z;
                const float len = std::sqrt(dx * dx + dz * dz);
                if (len > 1e-4f && dt > 0.0f) {
                    const float step = (std::min)(moveSpeed * dt, len);    // 向かう点を通り過ぎない
                    t->position.x += dx / len * step;
                    t->position.z += dz / len * step;
                    t->rotationEuler.y = std::atan2(dx, dz) * 57.2958f;    // 進む向きを向く
                    movedSpeed = step / dt;
                }
            }
            else {
                // 止まっている間は、相手のほうを向く
                const float fx = lt->position.x - t->position.x;
                const float fz = lt->position.z - t->position.z;
                if (fx * fx + fz * fz > 0.01f) t->rotationEuler.y = std::atan2(fx, fz) * 57.2958f;
            }
        }

        // 4) 地面と重力。地面の判定は全部のメッシュの三角形を調べて重いので、
        //    「横に動いたとき」と「空中にいるとき」だけ行う（止まって立っている間は調べない）
        const bool movedXZ = (t->position.x != before.x || t->position.z != before.z);
        if (movedXZ || !grounded) {
            velY -= 15.0f * dt;
            float ny = t->position.y + velY * dt;
            float gy;
            if (FindGround(ctx, self, t->position, gy) && ny <= gy) {
                ny = gy;
                velY = 0.0f;
                grounded = true;
            }
            else {
                grounded = false;
            }
            t->position.y = ny;
        }

        // 5) アニメーションへ：いまの移動の速さ
        SetAnimValue(self.GetComponent<AnimatorComponent>(), kAnimSpeed, movedSpeed);
    }

} // namespace uel
