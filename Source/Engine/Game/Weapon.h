#pragma once
#include "Engine/Game/Role.h"
#include <DirectXMath.h>
#include <cstdint>

namespace uel {
    struct GameContext;

    // 武器の値（3-3）。「その武器で何ができるか」だけを持つ、変わらないデータ
    // （UE の武器の Data Asset、Lyra の Ranged Weapon の設定に当たる）。
    // ロールごとの値は WeaponForRole の表（Weapon.cpp）の1か所にまとめてある。キャラクターのモデルが決まったら、表の値だけを差し替える。
    struct WeaponStats {
        float damage = 34.0f;           // 1発のダメージ（頭はヒットボックスの倍率が掛かる）
        int   pellets = 1;              // 1回撃つと出る弾の数（ショットガンなら 8 など）。1発ごとに damage が入る
        float fireInterval = 0.25f;     // 撃つ間隔（秒）
        float spreadDeg = 0.0f;         // 弾のずれ（度）。狙った向きから、この角度の円の中にばらける
        int   magazineSize = 12;        // 装弾数（弾は無限。マガジン単位でリロードする）
        float reloadTime = 1.5f;        // リロードにかかる時間（秒）
        float range = 100.0f;           // 弾が届く距離（m）
        bool  automatic = true;         // true：押している間撃ち続ける / false：1回押すと1発（セミオート）
    };

    // 武器の今の状態（弾の残り・リロード・次に撃てるまで）。Play 中だけの値で、保存しない。
    // 武器を持っている物（ロールの Component）が1つずつ持つ（UE でも弾数は武器のインスタンスが持つ）
    struct WeaponState {
        int   ammo = -1;                // マガジンの残り（-1 = まだ満タンにしていない。最初の UpdateWeapon で満タンにする）
        float cooldown = 0.0f;          // 次に撃てるまでの残り時間
        float reloadTimer = 0.0f;       // リロードの残り時間（0 以下 = リロードしていない）
        std::uint32_t rng = 0;          // 弾のずれに使う乱数の状態（0 = まだ種を決めていない）

        bool IsReloading() const { return reloadTimer > 0.0f; }
    };

    // 1回撃った結果
    struct WeaponFireResult {
        bool fired = false;             // 撃った（弾を1つ使った）
        bool damaged = false;           // どれかの弾が、ダメージの入る相手に当たった（ヒットマーカーを出す）
        bool headshot = false;          // どれかの弾が頭に当たった
        DirectX::XMFLOAT3 end{ 0.0f, 0.0f, 0.0f };     // 最初の弾が止まった所（外れたら射程の先。弾の線を描くため）
    };

    // ロールの武器の値（表。今は3ロールとも同じ仮の値。モデルが決まったら値を決める）
    const WeaponStats& WeaponForRole(Role role);

    // ロールの無いキャラ（ロール選択を通らない Scene など）が使う武器の値
    const WeaponStats& DefaultWeapon();

    // 毎フレーム1回呼ぶ：次に撃てるまでの時間を進め、リロードが終わったら満タンにする
    void UpdateWeapon(const WeaponStats& stats, WeaponState& state, float dt);

    // 今撃てるか（リロード中でない・間隔があいた・弾がある）
    bool CanFire(const WeaponStats& stats, const WeaponState& state);

    // リロードを始める（満タン・リロード中なら何もしない）。始めたら true
    bool StartReload(const WeaponStats& stats, WeaponState& state);

    // origin から dir（長さ 1）へ撃つ。撃てないときは何もしない（fired = false）。
    // 弾を pellets 発出し（それぞれ spreadDeg の中でずらす）、当たり方とダメージは WeaponTrace の TraceShot / ApplyShotDamage。
    // 弾を使い切ったら、自動でリロードを始める。撃った本人は ctx.self
    WeaponFireResult FireWeapon(GameContext& ctx, const WeaponStats& stats, WeaponState& state,
        const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir);

    // 向き dir（長さ 1）を、半角 spreadDeg 度の円錐の中でずらす（UE の FMath::VRandCone に当たる）。rng は乱数の状態（0 にしない）
    DirectX::XMFLOAT3 SpreadDirection(const DirectX::XMFLOAT3& dir, float spreadDeg, std::uint32_t& rng);

} // namespace uel
