#include "Engine/Components/AllyComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/Team.h"
#include "Engine/Game/WeaponTrace.h"
#include "Engine/Game/Weapon.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Components/PlayerComponent.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Components/HitboxComponent.h"
#include "Engine/Components/SupportComponent.h"
#include "Engine/Components/RoleComponent.h"
#include "Engine/Components/LifeStateComponent.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include "Engine/AI/EngineNavGrid.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace uel {
    namespace {

        // 今の敵より、これ（m）以上近い敵がいるときだけ乗り換える（敵の OpponentComponent と同じ考え方）
        constexpr float kRetargetMargin = 1.0f;
        // 見えなかった敵を、候補から外しておく時間（秒）。この間は、その敵へ乗り換えようとしない（レイも飛ばさない）
        constexpr float kBlockedMemory = 1.0f;
        // 回復している相手より、HP の割合がこれ以上低い味方がいるときだけ乗り換える（0.2 = 20%）
        constexpr float kHealSwitchMargin = 0.2f;
        // タクティカルリロード（3-5）：狙う敵がいない状態がこの時間（秒）続き、弾の残りが装弾数のこの割合以下なら詰め替える。
        // 少し待つのは、敵を倒して次の敵へ乗り換える一瞬にリロードを始めないため
        constexpr float kTacticalReloadDelay = 1.0f;
        constexpr float kTacticalReloadRatio = 0.25f;
        // 蘇生（4-3）：起こしに行く相手を探す距離（m。水平）、止まる距離、起こせる距離（プレイヤーと同じ 2m）
        constexpr float kReviveSearchRange = 40.0f;
        constexpr float kReviveReach = 1.2f;
        constexpr float kReviveRange = 2.0f;

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

        // 狙える敵か：敵対する陣営で、HP が残っている（倒れている最中の敵は HP 0 なので入らない）
        bool IsTargetable(const Entity& self, const Entity& e)
        {
            if (!IsHostile(self, e)) return false;
            auto* hp = e.GetComponent<HPComponent>();
            return hp && hp->hp > 0.0f;
        }

        // 射程 range の中で、いちばん近い狙える敵。avoid（少し前に見えなかった敵）は選ばない。
        // 距離だけで選び、見えるかどうか（レイ）はここでは調べない（候補ごとにレイを飛ばすと重いので、選んだ1体だけを調べる）
        Entity* FindNearestEnemy(GameContext& ctx, const Entity& self, const XMFLOAT3& from, float range, EntityId avoid, float& outDist)
        {
            Entity* best = nullptr;
            float bestDist = range;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot || slot.get() == &self || slot->id == avoid || !IsTargetable(self, *slot)) continue;
                auto* et = slot->GetComponent<TransformComponent>();
                if (!et) continue;
                const float d = DistXZ(et->position, from);
                if (d <= bestDist) { bestDist = d; best = slot.get(); }
            }
            outDist = bestDist;
            return best;
        }

        // 狙う点：ヒットボックスがあれば胴体の真ん中、無ければ足元から 1m 上
        XMFLOAT3 AimPoint(const Entity& e, const TransformComponent& et)
        {
            if (auto* hb = e.GetComponent<HitboxComponent>()) {
                const auto ws = hb->GetWorldShapes(et);
                return { (ws.bodyA.x + ws.bodyB.x) * 0.5f, (ws.bodyA.y + ws.bodyB.y) * 0.5f, (ws.bodyA.z + ws.bodyB.z) * 0.5f };
            }
            return { et.position.x, et.position.y + 1.0f, et.position.z };
        }

        // 撃つ位置 origin から敵が見えるか（間に壁や味方がいないか）。弾と同じ線（TraceShot）で調べ、最初に当たった物がその敵なら見える。
        // 見えるときは、狙う点への向き（長さ 1）と距離も返す。撃つ本人（ctx.self）は線に当たらない
        bool CanSee(GameContext& ctx, Entity& enemy, const XMFLOAT3& origin, XMFLOAT3& outDir, float& outLen)
        {
            auto* et = enemy.GetComponent<TransformComponent>();
            if (!et) return false;
            const XMFLOAT3 aim = AimPoint(enemy, *et);
            XMFLOAT3 d = { aim.x - origin.x, aim.y - origin.y, aim.z - origin.z };
            const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (len < 1e-4f) return false;
            outDir = { d.x / len, d.y / len, d.z / len };
            outLen = len;
            return TraceShot(ctx, origin, outDir, len + 1.0f).entity == &enemy;
        }

        // HP の割合（0〜1）。HP を持たない物は -1
        float HpRatio(const Entity& e)
        {
            auto* hp = e.GetComponent<HPComponent>();
            if (!hp || hp->maxHp <= 0.0f) return -1.0f;
            return hp->hp / hp->maxHp;
        }

        // 回復できる相手か：自分か味方の陣営で、HP が残っている（HP 0 の戦闘不能は回復しない。蘇生は別の仕組み＝段階 4）
        bool IsHealable(const Entity& self, const Entity& e)
        {
            if (&e != &self && !IsFriendly(self, e)) return false;
            return HpRatio(e) > 0.0f;
        }

        // 起こしに行く相手（4-3）：range の中で、ダウンしている味方（プレイヤーも）のうち、いちばん近い1人。
        // ほかの人が起こしている・起こしに向かっている（予約している）相手は選ばない
        Entity* FindDownedToRevive(GameContext& ctx, const Entity& self, const XMFLOAT3& from, float range)
        {
            Entity* best = nullptr;
            float bestDist = range;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot || slot.get() == &self || !IsFriendly(self, *slot)) continue;
                auto* ls = slot->GetComponent<LifeStateComponent>();
                auto* st = slot->GetComponent<TransformComponent>();
                if (!ls || !ls->IsDowned() || ls->IsTakenByOther(self.id) || !st) continue;
                const float d = DistXZ(st->position, from);
                if (d <= bestDist) { bestDist = d; best = slot.get(); }
            }
            return best;
        }

    } // namespace

    void AllyComponent::Update(GameContext& ctx)
    {
        Entity& self = ctx.self;
        auto* t = self.GetComponent<TransformComponent>();
        if (!t) return;
        const float dt = ctx.dt;
        AnimatorComponent* an = self.GetComponent<AnimatorComponent>();     // 無くても動く（アニメが変わらないだけ）

        // 0) 戦闘不能：HP が 0 になったら、その場で止まる（追いかけない・向きも変えない・撃たない）。
        //    HP が戻ったら（蘇生）、また動き出す。ダウン・Dead の状態と時間は LifeStateComponent が持つ（段階 4）
        auto* myHp = self.GetComponent<HPComponent>();
        const bool down = (myHp && myHp->hp <= 0.0f);
        if (down != incapacitated) {
            incapacitated = down;
            following = false;
            path.clear();
            targetId = EntityId{};
            healTargetId = EntityId{};
            reviveTargetId = EntityId{};
            SetAnimValue(an, kAnimDead, down ? 1.0f : 0.0f);    // Bool なので、変わったときだけ書けば値が残る（毎フレーム名前で探さない）
        }

        // 1) 追いかける相手。覚えている相手がいなくなったときだけ World を探し直す（毎フレーム全部を調べない）
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

        // 1a) 起こしに行く相手（4-3。AI の Phase 2）。ダウンした味方（プレイヤーも）がいれば、プレイヤーへの追従と回復より先に、
        //     駆け寄って起こす（L4D2 のボットと同じ。ユーザーと決めた）。
        //     向かう前に、相手の LifeStateComponent に予約する（ReserveRevive）。予約が生きている相手は、もう1人の AI は選ばない
        //     （2人とも駆け寄らず、1人は戦い続ける。UE の AI でよく使う「予約」）。予約は向かっている間、毎フレーム更新する。
        //     探すのは searchInterval ごと。起こしている相手を、ほかの人（プレイヤー）が先に起こし始めたら、あきらめる
        Entity* reviveE = ctx.world.Get(reviveTargetId);
        LifeStateComponent* reviveLife = reviveE ? reviveE->GetComponent<LifeStateComponent>() : nullptr;
        if (reviveTargetId.IsValid()) {
            const bool keep = !incapacitated && reviveLife && reviveLife->IsDowned()
                && !reviveLife->IsTakenByOther(self.id) && reviveLife->ReserveRevive(self.id);
            if (!keep) {
                reviveE = nullptr;
                reviveLife = nullptr;
                reviveTargetId = EntityId{};
                reviveSearchTimer = 0.0f;       // すぐ次の相手を探す（もう1人ダウンしているかもしれない）
                following = false;              // 追従を、今の位置から決め直す
                path.clear();
            }
        }
        reviveSearchTimer -= dt;
        if (!incapacitated && !reviveLife && reviveSearchTimer <= 0.0f) {
            reviveSearchTimer = searchInterval;
            Entity* found = FindDownedToRevive(ctx, self, t->position, kReviveSearchRange);
            LifeStateComponent* fl = found ? found->GetComponent<LifeStateComponent>() : nullptr;
            if (fl && fl->ReserveRevive(self.id)) {
                reviveE = found;
                reviveLife = fl;
                reviveTargetId = found->id;
                following = true;               // すぐ向かう（経路もすぐ作り直す）
                repathTimer = 0.0f;
                path.clear();
            }
        }
        TransformComponent* rt = reviveE ? reviveE->GetComponent<TransformComponent>() : nullptr;

        // 1b) 狙う敵。searchInterval ごとに、射程の中のいちばん近い敵を選び直す（毎フレームは探さない）。
        //     今の敵が倒れた・消えたときは、次のフレームですぐ選び直す。
        //     今の敵より kRetargetMargin 以上近い敵がいるときだけ乗り換える（同じくらいの距離の2体の間で、狙いが行ったり来たりしない）。
        //     乗り換える前に、その敵が見えるかを1回だけ調べる。見えなければ乗り換えず、その敵を kBlockedMemory 秒は候補から外す
        //     （壁や味方の向こうの近い敵と、見えている遠い敵の間で、狙いが行ったり来たりしない）
        const XMFLOAT3 muzzle = { t->position.x, t->position.y + aimHeight * t->scale.y, t->position.z };    // 撃つ位置（このフレームの最初の位置）
        Entity* target = ctx.world.Get(targetId);
        if (targetId.IsValid() && (!target || !IsTargetable(self, *target))) {
            target = nullptr;
            targetId = EntityId{};
            searchTimer = 0.0f;
        }
        if (blockedTimer > 0.0f) blockedTimer -= dt;
        if (blockedTimer <= 0.0f) blockedId = EntityId{};
        searchTimer -= dt;
        if (!incapacitated && searchTimer <= 0.0f) {
            searchTimer = searchInterval;
            float curDist = 1e9f;
            if (target) {
                if (auto* ct = target->GetComponent<TransformComponent>()) curDist = DistXZ(ct->position, t->position);
                if (curDist > attackRange) target = nullptr;        // 射程の外へ出た
            }
            float foundDist = 0.0f;
            Entity* found = FindNearestEnemy(ctx, self, t->position, attackRange, blockedId, foundDist);
            if (found && found != target && (!target || foundDist < curDist - kRetargetMargin)) {
                XMFLOAT3 dir; float len;
                if (CanSee(ctx, *found, muzzle, dir, len)) {
                    target = found;
                    attackTimer = (std::max)(attackTimer, reactionTime);    // 新しい敵に構え直す時間
                }
                else {
                    blockedId = found->id;                                  // 見えない：しばらく候補から外す
                    blockedTimer = kBlockedMemory;
                }
            }
            targetId = target ? target->id : EntityId{};
        }
        TransformComponent* tt = target ? target->GetComponent<TransformComponent>() : nullptr;

        // 1c) 回復する相手（SupportComponent があるときだけ。2-3）。
        //     HP の割合が healStartRatio 以下の味方（自分も含む）の中で、いちばん低い相手を選ぶ。
        //     始めたら healStopRatio（満タン）まで続ける（始める値と止める値を分けて、行ったり来たりしない）。
        //     選ぶのは searchInterval ごと。乗り換えるのは、今の相手より kHealSwitchMargin 以上低い味方がいるときだけ。
        //     線が壁にさえぎられていないか（IsLineBlocked）は、選んだ相手と今の相手だけを、選び直すときに調べる（毎フレームは調べない）
        const SupportComponent* support = self.GetComponent<SupportComponent>();
        Entity* healTarget = ctx.world.Get(healTargetId);
        if (healTargetId.IsValid()) {
            // 毎フレームの確かめ（軽い物だけ）：いなくなった・戦闘不能・満タン・届かない・起こしに行く、なら止める
            bool keep = support && !incapacitated && !reviveLife && healTarget && IsHealable(self, *healTarget)
                && HpRatio(*healTarget) < support->healStopRatio;
            if (keep && healTarget != &self) {
                auto* ht = healTarget->GetComponent<TransformComponent>();
                keep = ht && DistXZ(ht->position, t->position) <= support->healRange;
            }
            if (!keep) {
                healTarget = nullptr;
                healTargetId = EntityId{};
                healSearchTimer = 0.0f;                                 // すぐ次の相手を探す
                attackTimer = (std::max)(attackTimer, reactionTime);    // 攻撃に持ち替える時間
            }
        }
        healSearchTimer -= dt;
        if (support && !incapacitated && !reviveLife && healSearchTimer <= 0.0f) {     // 起こしに行く間は回復しない（蘇生が先）
            healSearchTimer = searchInterval;
            // 今の相手との間に壁が入ったら止める
            if (healTarget && healTarget != &self) {
                auto* ht = healTarget->GetComponent<TransformComponent>();
                if (!ht || IsLineBlocked(ctx, muzzle, AimPoint(*healTarget, *ht))) {
                    healTarget = nullptr;
                    attackTimer = (std::max)(attackTimer, reactionTime);
                }
            }
            // いちばん HP の割合が低い味方（始める値以下、届く距離）
            Entity* best = nullptr;
            float bestRatio = support->healStartRatio;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot || !IsHealable(self, *slot)) continue;
                const float r = HpRatio(*slot);
                if (r > bestRatio) continue;
                if (slot.get() != &self) {
                    auto* ht = slot->GetComponent<TransformComponent>();
                    if (!ht || DistXZ(ht->position, t->position) > support->healRange) continue;
                }
                bestRatio = r;
                best = slot.get();
            }
            if (best && best != healTarget && (!healTarget || bestRatio < HpRatio(*healTarget) - kHealSwitchMargin)) {
                bool visible = true;
                if (best != &self) {
                    auto* bt = best->GetComponent<TransformComponent>();
                    visible = !IsLineBlocked(ctx, muzzle, AimPoint(*best, *bt));
                }
                if (visible) healTarget = best;
            }
            healTargetId = healTarget ? healTarget->id : EntityId{};
        }
        TransformComponent* ht = (healTarget && healTarget != &self) ? healTarget->GetComponent<TransformComponent>() : nullptr;

        const XMFLOAT3 before = t->position;
        float movedSpeed = 0.0f;
        // 向かう相手：起こす相手がいればその人（kReviveReach まで近づく）、いなければプレイヤー（followDistance まで）
        TransformComponent* goalT = rt ? rt : lt;
        const float stopDist = rt ? kReviveReach : followDistance;
        const float goDist = rt ? kReviveReach : resumeDistance;
        if (goalT && !incapacitated) {
            // 2) 動くか止まるか。止まる距離と動き出す距離を分けて、境目で動く・止まるを繰り返さないようにする
            //    （起こす相手は動かないので、同じ距離でよい）
            const float dist = DistXZ(t->position, goalT->position);
            if (following && dist <= stopDist) { following = false; path.clear(); }
            else if (!following && dist > goDist) { following = true; repathTimer = 0.0f; }

            if (following) {
                // 3) 経路：一定の間隔で、しかも相手が少し動いていたときだけ計算し直す（A* を毎フレーム回さない）
                repathTimer -= dt;
                if (ctx.nav && repathTimer <= 0.0f) {
                    repathTimer = repathInterval;
                    if (path.empty() || DistXZ(pathGoal, goalT->position) > 0.5f) {
                        ctx.nav->FindPath(t->position, goalT->position, path);
                        pathGoal = goalT->position;
                        wpIndex = (path.size() > 1) ? 1 : 0;    // 最初の点は自分のいるマスなので飛ばす
                    }
                }

                // 次に向かう点。経路が無い（グリッドの外など）ときは、相手へまっすぐ向かう
                XMFLOAT3 goal = goalT->position;
                if (!path.empty()) {
                    if (wpIndex >= path.size()) wpIndex = path.size() - 1;
                    if (DistXZ(path[wpIndex], t->position) < 0.5f && wpIndex + 1 < path.size()) ++wpIndex;
                    goal = path[wpIndex];
                }

                const float dx = goal.x - t->position.x;
                const float dz = goal.z - t->position.z;
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
                // 止まっている間は、起こしている味方 → 回復している味方 → 狙っている敵 → 向かう相手、の順に、いる相手のほうを向く
                const XMFLOAT3& face = rt ? rt->position : (ht ? ht->position : (tt ? tt->position : goalT->position));
                const float fx = face.x - t->position.x;
                const float fz = face.z - t->position.z;
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

        // 5) 回復と攻撃（同時には使わない。回復が先＝回復する相手がいれば撃たない）。
        //    攻撃：撃てる時間になったら、もう一度敵が見えるかを調べ（選んだ後に壁の陰へ入ったかもしれない）、見えていれば撃つ。
        //    見えなければ撃たず、その敵をしばらく候補から外して、次のフレームで選び直す（壁の向こうの敵にこだわらない）。
        //    レイを飛ばすのは「撃つとき」と「乗り換えるとき」だけ（1発につき、見えるかの確認と本番の2本）。
        //    撃てるのは、AI の側の時間（attackTimer）と武器の側（CanFire：リロード中でない・間隔が過ぎた・弾がある）の両方がそろったとき。
        //    だから撃つ間隔は「武器の間隔」と「AI の撃つ間隔の下限」の遅いほうになる（3-5）
        if (shotTime > 0.0f) shotTime -= dt;
        attackTimer -= dt;
        bool fired = false;

        // 武器（3-5）：ロールの Component があればロールの武器（値は表、状態はロールの Component）、無ければ既定の武器。
        // プレイヤーと同じ関数（Game/Weapon）で、時間を進め・撃ち・リロードする
        RoleComponent* role = self.GetComponent<RoleComponent>();
        const WeaponStats& weapon = role ? role->GetWeapon() : DefaultWeapon();
        WeaponState& weaponState = role ? role->weaponState : fallbackWeapon;
        if (!incapacitated) UpdateWeapon(weapon, weaponState, dt);      // 戦闘不能の間は、リロードも進まない

        // タクティカルリロード：狙う敵がいない状態が kTacticalReloadDelay 秒続き、弾が装弾数の 1/4 以下なら、次の戦いの前に詰め替える
        if (target) idleTime = 0.0f;
        else        idleTime += dt;
        if (!incapacitated && idleTime >= kTacticalReloadDelay && weaponState.ammo >= 0
            && weaponState.ammo <= static_cast<int>(weapon.magazineSize * kTacticalReloadRatio))
            StartReload(weapon, weaponState);       // リロード中・満タンなら何もしない

        // 5-0) 蘇生（4-3）：起こす相手に kReviveRange まで近づいたら、起こす（続けている間、毎フレーム進める）。
        //      起こしている間は撃たない・回復しない（手がふさがる）。攻撃されても続ける（ユーザーが選んだ）
        bool revivingNow = false;
        if (reviveLife && rt && !incapacitated && !following && DistXZ(rt->position, t->position) <= kReviveRange) {   // 着いて止まってから
            reviveLife->AddReviveProgress(self.id, dt);
            revivingNow = true;
            if (attackTimer < reactionTime) attackTimer = reactionTime;     // 起こし終わってから撃つまでの、持ち替えの時間
        }

        // 5a) 回復：線でつないでいる間、毎秒 healPerSecond ずつ回復する（満タンを超えない）。回復している間は撃たない
        if (revivingNow) {
            // 起こしている：撃たない・回復しない
        }
        else if (healTarget && support) {
            if (auto* hhp = healTarget->GetComponent<HPComponent>())
                hhp->hp = (std::min)(hhp->maxHp, hhp->hp + support->healPerSecond * dt);
            healFrom = { t->position.x, t->position.y + aimHeight * t->scale.y, t->position.z };     // 動いた後の位置
            if (ht) healTo = AimPoint(*healTarget, *ht);
            else    healTo = AimPoint(self, *t);                                                    // 自分を回復：自分の胴体
            if (attackTimer < reactionTime) attackTimer = reactionTime;     // 回復が終わってから撃つまでの、持ち替えの時間
        }
        else if (tt && !incapacitated && attackTimer <= 0.0f && CanFire(weapon, weaponState)) {
            const XMFLOAT3 origin = { t->position.x, t->position.y + aimHeight * t->scale.y, t->position.z };    // 動いた後の位置から撃つ
            XMFLOAT3 dir; float len;
            if (CanSee(ctx, *target, origin, dir, len)) {
                // 見えている：狙う向きを AI の腕前（aimSpread）でずらしてから、武器で撃つ（武器のずれ・弾の数・ダメージは FireWeapon の中）。
                // 外れることもある。当たり方・ダメージはプレイヤーと同じ関数
                if (rng == 0) rng = ((self.id.index + 1u) * 2654435761u) ^ (self.id.generation * 40503u) ^ 0x9E3779B9u;  // Entity ごとに違う種
                if (rng == 0) rng = 1u;
                const XMFLOAT3 aimDir = SpreadDirection(dir, aimSpread, rng);
                const WeaponFireResult fire = FireWeapon(ctx, weapon, weaponState, origin, aimDir);
                if (fire.fired) {
                    shotFrom = origin;
                    shotTo = fire.end;              // 最初の弾が止まった所（外れたら武器の射程の先）
                    shotTime = 0.08f;
                    attackTimer = attackInterval;   // AI の撃つ間隔の下限
                    fired = true;
                }
            }
            else {
                // 見えない：この敵はしばらく候補から外して、次のフレームで選び直す
                blockedId = target->id;
                blockedTimer = kBlockedMemory;
                targetId = EntityId{};
                searchTimer = 0.0f;
            }
        }

        // HUD に出す武器の値（HUD がロールの Component を探さなくて済むように、ここで覚えておく）
        hudAmmo = weaponState.ammo;
        hudMagazine = weapon.magazineSize;
        hudReloading = weaponState.IsReloading();

        // 6) アニメーションへ：いまの移動の速さ、撃ったこと、回復しているか
        SetAnimValue(an, kAnimSpeed, movedSpeed);
        if (fired) SetAnimValue(an, kAnimAttack, 1.0f);    // Trigger
        const bool healingNow = healTargetId.IsValid();
        if (healingNow != healAnim) {                        // Bool：変わったときだけ書く
            healAnim = healingNow;
            SetAnimValue(an, kAnimHeal, healingNow ? 1.0f : 0.0f);
        }
        reviving = revivingNow;
        if (revivingNow != reviveAnim) {                     // Bool：変わったときだけ書く（グラフに Revive が無ければ何もしない）
            reviveAnim = revivingNow;
            SetAnimValue(an, kAnimRevive, revivingNow ? 1.0f : 0.0f);
        }
    }

} // namespace uel
