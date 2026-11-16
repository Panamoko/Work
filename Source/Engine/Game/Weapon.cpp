#include "Engine/Game/Weapon.h"
#include "Engine/Game/WeaponTrace.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Scene/Entity.h"
#include <cmath>

using namespace DirectX;

namespace uel {
    namespace {

        // ロールの武器の表（UE の DataTable に当たる）。並びは Role と同じ（Tank, DPS, Support）。
        // ★今は3ロールとも同じ仮の値（今までのプレイヤーの射撃＝34 ダメージ・ずれ 0 に、連射・装弾数・リロードを足しただけ）。
        //   各ロールの武器の種類と値は、キャラクターのモデルの見た目に合わせて決める（ユーザーが決める。0章「決まっていないこと」）
        const WeaponStats kRoleWeapons[kRoleCount] = {
            //  damage  pellets  interval  spread  magazine  reload  range   automatic
            {   34.0f,  1,       0.25f,    0.0f,   12,       1.5f,   100.0f, true },   // Tank（仮）
            {   34.0f,  1,       0.25f,    0.0f,   12,       1.5f,   100.0f, true },   // DPS（仮）
            {   34.0f,  1,       0.25f,    0.0f,   12,       1.5f,   100.0f, true },   // Support（仮）
        };

        const WeaponStats kDefaultWeapon{};    // ロールの無いキャラ：WeaponStats の初期値（上の仮の値と同じ）

        // 0 以上 1 未満の乱数（xorshift32。UE の FRandomStream のように、武器ごとに自分の乱数の状態を持つ）
        float Rand01(std::uint32_t& s)
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return static_cast<float>(s >> 8) * (1.0f / 16777216.0f);
        }

    } // namespace

    const WeaponStats& WeaponForRole(Role role)
    {
        const int i = static_cast<int>(role);
        return (i >= 0 && i < kRoleCount) ? kRoleWeapons[i] : kDefaultWeapon;
    }

    const WeaponStats& DefaultWeapon()
    {
        return kDefaultWeapon;
    }

    void UpdateWeapon(const WeaponStats& stats, WeaponState& state, float dt)
    {
        if (state.ammo < 0) state.ammo = stats.magazineSize;       // 初めて使う：満タンから始める
        if (state.cooldown > 0.0f) state.cooldown -= dt;
        if (state.reloadTimer > 0.0f) {
            state.reloadTimer -= dt;
            if (state.reloadTimer <= 0.0f) {                        // リロードが終わった：満タン
                state.reloadTimer = 0.0f;
                state.ammo = stats.magazineSize;
            }
        }
    }

    bool CanFire(const WeaponStats& stats, const WeaponState& state)
    {
        (void)stats;
        return !state.IsReloading() && state.cooldown <= 0.0f && state.ammo > 0;
    }

    bool StartReload(const WeaponStats& stats, WeaponState& state)
    {
        if (state.IsReloading() || state.ammo >= stats.magazineSize) return false;
        state.reloadTimer = (stats.reloadTime > 0.0f) ? stats.reloadTime : 0.0001f;    // 0 秒でも、次の UpdateWeapon で満タンになる
        return true;
    }

    WeaponFireResult FireWeapon(GameContext& ctx, const WeaponStats& stats, WeaponState& state,
        const XMFLOAT3& origin, const XMFLOAT3& dir)
    {
        WeaponFireResult result;
        if (!CanFire(stats, state)) return result;

        if (state.rng == 0) {      // Entity ごとに違う種（同じフレームに撃った2人の弾が、同じずれ方をしないように）
            state.rng = ((ctx.self.id.index + 1u) * 2654435761u) ^ (ctx.self.id.generation * 40503u) ^ 0x9E3779B9u;
            if (state.rng == 0) state.rng = 1u;
        }

        const int pellets = (stats.pellets > 0) ? stats.pellets : 1;
        for (int i = 0; i < pellets; ++i) {
            const XMFLOAT3 d = SpreadDirection(dir, stats.spreadDeg, state.rng);
            const ShotHit hit = TraceShot(ctx, origin, d, stats.range);
            if (ApplyShotDamage(ctx, hit, stats.damage)) {         // 味方・壁にはダメージが入らない
                result.damaged = true;
                if (hit.headshot) result.headshot = true;
            }
            if (i == 0) {
                const float len = hit.entity ? hit.distance : stats.range;
                result.end = { origin.x + d.x * len, origin.y + d.y * len, origin.z + d.z * len };
            }
        }

        result.fired = true;
        state.ammo -= 1;
        state.cooldown = stats.fireInterval;
        if (state.ammo <= 0) StartReload(stats, state);             // 撃ち切った：自動でリロード
        return result;
    }

    XMFLOAT3 SpreadDirection(const XMFLOAT3& dir, float spreadDeg, std::uint32_t& rng)
    {
        if (spreadDeg <= 0.0f) return dir;
        const XMVECTOR d = XMLoadFloat3(&dir);
        // dir に垂直な2本の軸（dir が真上・真下に近いときは、X 軸を元に作る）
        const XMVECTOR base = (std::fabs(dir.y) < 0.99f) ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
        const XMVECTOR right = XMVector3Normalize(XMVector3Cross(base, d));
        const XMVECTOR up = XMVector3Cross(d, right);
        // 円の中に一様に散らす（半径は乱数の平方根。そうしないと真ん中に集まりすぎる）
        const float radius = std::tan(XMConvertToRadians(spreadDeg)) * std::sqrt(Rand01(rng));
        const float angle = XM_2PI * Rand01(rng);
        XMVECTOR v = XMVectorAdd(d, XMVectorScale(right, radius * std::cos(angle)));
        v = XMVectorAdd(v, XMVectorScale(up, radius * std::sin(angle)));
        XMFLOAT3 out;
        XMStoreFloat3(&out, XMVector3Normalize(v));
        return out;
    }

} // namespace uel
