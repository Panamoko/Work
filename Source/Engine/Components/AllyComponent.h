#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Scene/EntityId.h"
#include "Engine/Game/Weapon.h"
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace uel {

    // AI 仲間の「頭」（UE の AIController に当たる）。どのロールでも共通の判断をここに書く。
    // 「何ができるか」（武器・アビリティ）は、ロールの Component（Tank / DPS / Support）が持つ。
    //
    // 今できること：
    //   追従：プレイヤーを経路探索で追いかけ、近くまで来たら止まる（1-1）。
    //   戦闘不能：HP が 0 になったら、その場で止まって倒れる（1-2c）。段階 4 で「ダウン（蘇生を待つ）」に置き換える。
    //   攻撃：射程の中のいちばん近い敵を選び、見えていれば撃つ。追いかけながらでも撃つ（2-2）。
    //         武器はロールの武器（ダメージ・装弾数・リロード・弾の数）を、プレイヤーと同じ関数（Game/Weapon）で撃つ（3-5）。
    //         AI の側では「撃つ間隔の下限」と「狙いのずれ」を足して、プレイヤーより少し弱くする。
    //   リロード：弾が切れたら自動で。敵がいない間に弾が 1/4 以下なら、先に詰め替える（タクティカルリロード。3-5）。
    //   回復：SupportComponent が付いていれば、HP の割合が低い味方（自分も含む）を線でつないで回復する。回復中は撃たない（2-3）。
    // まだ無いこと：蘇生、撃ちやすい位置・回復しやすい位置へ動くこと、壁との当たり判定（経路だけで避ける）。
    class AllyComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<AllyComponent>(*this); }
        void Update(GameContext& ctx) override;

        // ---- 追従の設定 ----
        float moveSpeed = 4.5f;         // プレイヤー（4.0）より少し速くして、離されても追いつけるようにする
        float followDistance = 3.0f;    // 追いかけている間、ここまで近づいたら止まる
        float resumeDistance = 4.5f;    // 止まっている間、ここより離れたら動き出す（止まる距離との差で、動く・止まるを繰り返さない）
        float repathInterval = 0.5f;    // 経路を計算し直す間隔（秒）

        // ---- 攻撃の設定（AI の撃ち方。武器の値はロールの武器の表＝Game/Weapon から取る。3-5）----
        float attackRange = 25.0f;      // この距離（m。水平）より近い敵だけを狙う（武器の射程とは別。AI が戦いを始める距離）
        float attackInterval = 0.8f;    // 撃つ間隔の下限（秒）。武器の間隔とこれの、遅いほうで撃つ（AI がプレイヤーより速く撃たないように）
        float aimSpread = 2.0f;         // 狙いのずれ（度）。武器のずれの前に、狙う向きをこの角度の円の中でずらす（AI の腕前）
        float aimHeight = 1.6f;         // 撃つ位置の高さ（足元から。プレイヤーの目の高さと同じ）
        float searchInterval = 0.3f;    // 狙う敵を選び直す間隔（秒）
        float reactionTime = 0.3f;      // 新しい敵を選んでから、最初に撃つまでの時間（秒。見つけた瞬間に撃たない）

        // アニメーショングラフに渡すパラメータの名前（Player / Opponent と同じ名前）
        static constexpr const char* kAnimSpeed = "Speed";      // Float  ：いまの移動の速さ（m/s）。止まっていれば 0
        static constexpr const char* kAnimDead = "Dead";        // Bool   ：戦闘不能（HP 0）。HP が戻るまで true のまま
        static constexpr const char* kAnimAttack = "Attack";    // Trigger：撃った
        static constexpr const char* kAnimHeal = "Heal";        // Bool   ：回復の線をつないでいる（グラフに無ければ何もしない）

        // HP が 0 で、戦闘不能か（動かない。撃たない。敵に狙われない。プレイヤーの弾を止めない）
        bool IsIncapacitated() const { return incapacitated; }

        // 今狙っている敵（無効 = 狙っていない）
        EntityId GetTarget() const { return targetId; }

        // 最後に撃った弾の線（HUD で描くため。2-2b）。GetShotTime() が 0 より大きい間だけ表示する
        const DirectX::XMFLOAT3& GetShotFrom() const { return shotFrom; }
        const DirectX::XMFLOAT3& GetShotTo() const { return shotTo; }
        float GetShotTime() const { return shotTime; }

        // 回復（Support のときだけ。2-3）。回復している間、線の両端（HUD で描くため。2-3b）
        bool IsHealing() const { return healTargetId.IsValid(); }
        EntityId GetHealTarget() const { return healTargetId; }
        const DirectX::XMFLOAT3& GetHealFrom() const { return healFrom; }
        const DirectX::XMFLOAT3& GetHealTo() const { return healTo; }

        // 武器（3-5。HUD に出す値）：マガジンの残り、装弾数、リロード中か
        int  GetAmmo() const { return hudAmmo; }
        int  GetMagazineSize() const { return hudMagazine; }
        bool IsReloading() const { return hudReloading; }

    private:
        // ---- 実行中の状態（保存しない）----
        EntityId leader;                            // 追いかける相手（プレイヤー）。毎フレーム World を探さないように覚えておく
        std::vector<DirectX::XMFLOAT3> path;        // 今の経路（マスの中心の並び）
        size_t   wpIndex = 0;                       // 次に向かう点
        float    repathTimer = 0.0f;
        DirectX::XMFLOAT3 pathGoal{ 0.0f, 0.0f, 0.0f };    // 今の経路を作ったときの相手の位置
        bool     following = false;                 // 今追いかけている（動いている）
        bool     grounded = false;                  // 地面に立っている（止まって立っている間は、地面を調べ直さない）
        float    velY = 0.0f;
        bool     incapacitated = false;             // 戦闘不能（HP 0）。変わったフレームだけアニメの Dead を書く

        // 攻撃
        EntityId targetId;                          // 今狙っている敵。毎フレーム World を探さないように覚えておく
        EntityId blockedId;                         // 少し前に見えなかった敵（壁や味方の向こう）。blockedTimer の間は選ばない
        float    blockedTimer = 0.0f;               // blockedId を候補から外しておく残り時間
        float    searchTimer = 0.0f;                // 次に狙う敵を選び直すまでの残り時間
        float    attackTimer = 0.0f;                // AI の側で、次に撃てるまでの残り時間（撃つ間隔の下限・構える時間・持ち替え）
        float    idleTime = 0.0f;                   // 狙う敵がいなくなってからの時間（タクティカルリロードの判断に使う）
        std::uint32_t rng = 0;                      // 狙いのずれに使う乱数の状態（0 = まだ種を決めていない）
        DirectX::XMFLOAT3 shotFrom{ 0.0f, 0.0f, 0.0f };    // 最後に撃った弾の線の始まり
        DirectX::XMFLOAT3 shotTo{ 0.0f, 0.0f, 0.0f };      // 最後に撃った弾の線の終わり（当たった所。外れたら射程の先）
        float    shotTime = 0.0f;                   // 弾の線を出しておく残り時間

        // 武器（3-5）
        WeaponState fallbackWeapon;                 // ロールの無いときに使う武器の状態（ロールがあれば、ロールの Component の weaponState を使う）
        int      hudAmmo = 0;                       // HUD に出す値（毎フレームの最後に覚える）
        int      hudMagazine = 0;
        bool     hudReloading = false;

        // 回復
        EntityId healTargetId;                      // 今回復している味方（無効 = 回復していない。自分のこともある）
        float    healSearchTimer = 0.0f;            // 次に回復する相手を選び直すまでの残り時間（searchInterval ごと）
        bool     healAnim = false;                  // アニメに渡した Heal の値（変わったときだけ書く）
        DirectX::XMFLOAT3 healFrom{ 0.0f, 0.0f, 0.0f };    // 回復の線の始まり（自分）
        DirectX::XMFLOAT3 healTo{ 0.0f, 0.0f, 0.0f };      // 回復の線の終わり（相手の胴体の真ん中）
    };

} // namespace uel
