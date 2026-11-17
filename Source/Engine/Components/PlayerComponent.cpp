#include "Engine/Components/PlayerComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/InputState.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Game/WeaponTrace.h"
#include "Engine/Game/Weapon.h"
#include "Engine/Game/Team.h"
#include "Engine/Components/RoleComponent.h"
#include "Engine/Components/SupportComponent.h"
#include "Engine/Components/HitboxComponent.h"
#include "Engine/Components/LifeStateComponent.h"

#include <algorithm>
#include <cmath>
#include <memory>

using namespace DirectX;

namespace uel {
    namespace {

        // 照準の線上に味方がいるかを調べ直す間隔（秒）。線はメッシュの三角形まで調べて重いので、毎フレームは飛ばさない。
        // 0.1 秒なら、照準を外してから線が切れるまでの遅れは目立たない
        constexpr float kHealTraceInterval = 0.1f;

        // 回復の線の終わり：ヒットボックスがあれば胴体の真ん中、無ければ足元から 1m 上（AllyComponent の AimPoint と同じ決め方）
        XMFLOAT3 HealPoint(const Entity& e, const TransformComponent& et)
        {
            if (auto* hb = e.GetComponent<HitboxComponent>()) {
                const auto ws = hb->GetWorldShapes(et);
                return { (ws.bodyA.x + ws.bodyB.x) * 0.5f, (ws.bodyA.y + ws.bodyB.y) * 0.5f, (ws.bodyA.z + ws.bodyB.z) * 0.5f };
            }
            return { et.position.x, et.position.y + 1.0f, et.position.z };
        }

        // 蘇生できる距離（m。水平）と、近くのダウンした味方を探し直す間隔（秒）
        constexpr float kReviveRange = 2.0f;
        constexpr float kReviveScanInterval = 0.1f;

        // XZ 平面での距離（高さの違いは見ない）
        float DistXZ(const XMFLOAT3& a, const XMFLOAT3& b)
        {
            const float dx = a.x - b.x, dz = a.z - b.z;
            return std::sqrt(dx * dx + dz * dz);
        }

        // from から range の中にいる、ダウンしている味方のうち、いちばん近い1人（いなければ無効）
        EntityId FindDownedAlly(GameContext& ctx, const Entity& self, const XMFLOAT3& from, float range)
        {
            EntityId best;
            float bestDist = range;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot || slot.get() == &self || !IsFriendly(self, *slot)) continue;
                auto* ls = slot->GetComponent<LifeStateComponent>();
                auto* st = slot->GetComponent<TransformComponent>();
                if (!ls || !ls->IsDowned() || !st) continue;
                const float d = DistXZ(st->position, from);
                if (d <= bestDist) { bestDist = d; best = slot->id; }
            }
            return best;
        }

    } // namespace

    void PlayerComponent::Update(GameContext& ctx)
    {
        Entity& self = ctx.self;
        auto* t = self.GetComponent<TransformComponent>();
        if (!t) return;
        const InputState& in = ctx.input;
        float dt = ctx.dt;
        if (hitMarker > 0.0f) hitMarker -= dt;
        auto* myHp = self.GetComponent<HPComponent>();
        const bool isDead = (myHp && myHp->hp <= 0.0f);     // HP 0（ダウン・Dead・状態を持たないときの死亡）
        const bool canAct = in.enabled && !isDead;          // HP 0 の間は、動けない・撃てない・回復できない
        // 見回しは、ダウンしている間もできる（仲間が助けに来るのを見られるように。段階 4）。Dead と、状態を持たないときの HP 0 はできない
        const LifeStateComponent* life = self.GetComponent<LifeStateComponent>();
        const bool canLook = in.enabled && (life ? !life->IsDead() : !isDead);

        // 蘇生（4-2）：ダウンしている味方の近く（kReviveRange）で E を押し続けると、蘇生が進む（LifeStateComponent の reviveTime 秒で立ち上がる）。
        //   相手は、近くにいるダウンした味方のうち、いちばん近い1人。探すのは kReviveScanInterval ごと（毎フレーム World を全部見ない）。
        //   蘇生している間は、動けない・撃てない・回復できない・リロードできない（見回しはできる）。攻撃されても続ける（ユーザーが選んだ）。
        //   ほかの人（AI 仲間）がもう蘇生している相手には、手を足さない（2人で蘇生しても速くならない）
        reviveScanTimer -= dt;
        if (!canAct) reviveCandidateId = EntityId{};
        else if (reviveScanTimer <= 0.0f) {
            reviveScanTimer = kReviveScanInterval;
            reviveCandidateId = FindDownedAlly(ctx, self, t->position, kReviveRange);
        }
        Entity* reviveE = ctx.world.Get(reviveCandidateId);
        LifeStateComponent* reviveLife = reviveE ? reviveE->GetComponent<LifeStateComponent>() : nullptr;
        const TransformComponent* reviveT = reviveE ? reviveE->GetComponent<TransformComponent>() : nullptr;
        if (!reviveLife || !reviveLife->IsDowned() || !reviveT || DistXZ(reviveT->position, t->position) > kReviveRange) {
            reviveCandidateId = EntityId{};     // 立ち上がった・Dead になった・離れた（毎フレームの軽い確かめ）
            reviveLife = nullptr;
        }
        const bool othersReviving = reviveLife && reviveLife->IsBeingRevived() && reviveLife->GetReviver() != self.id;
        const bool reviving = canAct && in.interactHeld && reviveLife && !othersReviving;
        if (reviving) reviveLife->AddReviveProgress(self.id, dt);
        reviveTargetId = reviving ? reviveCandidateId : EntityId{};
        hudReviveProgress = reviving ? reviveLife->GetReviveProgress() : 0.0f;
        const bool canMove = canAct && !reviving;           // 蘇生している間は、手がふさがる

        // 対象メッシュ取得（assetId空は手続き立方体）
        auto meshOf = [&](Entity* ge) -> std::shared_ptr<Mesh> {
            auto* gm = ge->GetComponent<MeshRendererComponent>();
            if (!gm) return nullptr;
            return gm->assetId.empty() ? ctx.meshCache.GetCube(ctx.device)
                : ctx.meshCache.GetOrLoad(ctx.device, gm->assetId);
            };

        // 見回し
        if (canLook && in.lookActive) {
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
        if (canMove) {
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
        const bool jumped = grounded && canMove && in.jumpPressed;
        if (jumped) velY = 6.0f;
        velY -= 15.0f * dt;
        t->position.y += velY * dt;
        if (t->position.y < ground) { t->position.y = ground; velY = 0.0f; }

        // 照準（目の位置と向き。回復の線と射撃で使う）
        float cp = cosf(pitch), sp = sinf(pitch);
        XMFLOAT3 eyePos = { t->position.x + sy * eyeForward, t->position.y + eyeHeight, t->position.z + cy * eyeForward };
        const XMFLOAT3 rd = { cp * sy, sp, cp * cy };     // 照準の向き（長さ 1）

        // 回復（3-2）：Support のときだけ。右クリックを押している間、照準の線上（healRange まで）にいる味方へ回復の線をつなぐ。
        //   線上を調べるのは弾と同じ線（TraceShot）。いちばん手前に当たった物が、HP の残っている味方なら回復する
        //   （壁・敵が手前にあれば回復しない。戦闘不能の味方は線が素通りする＝回復しない。蘇生は段階 4）。
        //   自分は回復しない（イラリーと同じ。ユーザーが選んだ）。調べ直すのは kHealTraceInterval ごと（その間は同じ相手を回復する）。
        //   右クリックを押している間は撃たない（攻撃と回復は同時に使わない。AI 仲間と同じ決まり）
        const SupportComponent* support = self.GetComponent<SupportComponent>();
        const bool healHeld = support && canMove && in.healHeld;
        if (!healHeld) {
            healTargetId = EntityId{};
            healTraceTimer = 0.0f;          // 次に押したら、すぐ調べる
        }
        else {
            healTraceTimer -= dt;
            if (healTraceTimer <= 0.0f) {
                healTraceTimer = kHealTraceInterval;
                const ShotHit hit = TraceShot(ctx, eyePos, rd, support->healRange);
                const Entity* h = hit.entity;
                const HPComponent* hhp = h ? h->GetComponent<HPComponent>() : nullptr;
                healTargetId = (h && IsFriendly(self, *h) && hhp && hhp->hp > 0.0f) ? h->id : EntityId{};
            }
            // 調べ直すまでの間も、毎フレーム軽い確かめだけはする（消えた・戦闘不能になった、なら止める）
            Entity* ht = ctx.world.Get(healTargetId);
            HPComponent* hhp = ht ? ht->GetComponent<HPComponent>() : nullptr;
            auto* htt = ht ? ht->GetComponent<TransformComponent>() : nullptr;
            if (!ht || !hhp || !htt || hhp->hp <= 0.0f) {
                healTargetId = EntityId{};
            }
            else {
                hhp->hp = (std::min)(hhp->maxHp, hhp->hp + support->healPerSecond * dt);   // 満タンを超えない
                healTo = HealPoint(*ht, *htt);
            }
        }

        // 武器（3-3）：ロールがあればロールの武器、無ければ既定の武器（ロール選択を通らない Scene でも撃てるように）。
        //   値（ダメージ・連射の間隔・装弾数・リロードなど）は Game/Weapon の表、弾の残りなどの状態はロールの Component が持つ。
        //   撃ち方・リロードの処理は Game/Weapon の共通の関数（AI 仲間も 3-5 で同じ関数を使う）。
        //   押している間撃ち続ける（automatic）か、1回押すと1発か、は武器の値で決まる。R でリロード、撃ち切ったら自動でリロード
        RoleComponent* role = self.GetComponent<RoleComponent>();
        const WeaponStats& weapon = role ? role->GetWeapon() : DefaultWeapon();
        WeaponState& weaponState = role ? role->weaponState : fallbackWeapon;
        UpdateWeapon(weapon, weaponState, dt);
        if (canMove && in.reloadPressed) StartReload(weapon, weaponState);

        const bool trigger = weapon.automatic ? in.shootHeld : in.shootClicked;
        bool shot = false;
        if (canMove && trigger && !healHeld) {
            // 弾の線が何に当たるか（UE の LineTrace）→ ダメージ（UE の ApplyPointDamage）は FireWeapon の中（Game/WeaponTrace の関数）
            const WeaponFireResult fire = FireWeapon(ctx, weapon, weaponState, eyePos, rd);
            shot = fire.fired;
            if (fire.damaged) {             // 味方・壁にはダメージが入らない（ヒットマーカーも出ない）
                hitMarker = 0.15f;
                lastHeadshot = fire.headshot;
            }
        }

        // HUD に出す値（毎フレーム覚えておく。HUD がロールの Component を探さなくて済むように）
        hudAmmo = weaponState.ammo;
        hudMagazine = weapon.magazineSize;
        hudReload = (weaponState.IsReloading() && weapon.reloadTime > 0.0f)
            ? 1.0f - weaponState.reloadTimer / weapon.reloadTime : -1.0f;

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
            an->params.SetIfExists(kAnimAimPitch, XMConvertToDegrees(pitch));        // 視点の上下（度）
            if (jumped) an->params.SetIfExists(kAnimJump, 1.0f);                    // Trigger：跳んだ
            if (shot)   an->params.SetIfExists(kAnimAttack, 1.0f);                  // Trigger：撃った
            const bool healingNow = healTargetId.IsValid();
            if (healingNow != healAnim) {                                           // Bool：変わったときだけ書く
                healAnim = healingNow;
                an->params.SetIfExists(kAnimHeal, healingNow ? 1.0f : 0.0f);
            }
        }

        // カメラ
        XMVECTOR look = XMVectorSet(cp * sy, sp, cp * cy, 0);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        XMVECTOR eye = XMVectorSet(eyePos.x, eyePos.y, eyePos.z, 0);
        XMStoreFloat4x4(&view, XMMatrixLookAtLH(eye, XMVectorAdd(eye, look), up));
    }

} // namespace uel