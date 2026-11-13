#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Scene/EntityId.h"
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace uel {

    // AI 仲間の「頭」（UE の AIController に当たる）。どのロールでも共通の判断をここに書く。
    // 「何ができるか」（武器・アビリティ）は、ロールの Component（Tank / DPS / Support）に持たせる予定。
    //
    // 今できること：
    //   追従：プレイヤーを経路探索で追いかけ、近くまで来たら止まる（1-1）。
    //   戦闘不能：HP が 0 になったら、その場で止まって倒れる（1-2c）。段階 4 で「ダウン（蘇生を待つ）」に置き換える。
    //   攻撃：射程の中のいちばん近い敵を選び、見えていれば一定の間隔で撃つ。追いかけながらでも撃つ（2-2）。
    // まだ無いこと：回復・蘇生、ロールごとの武器（段階 3）、撃ちやすい位置へ動くこと、壁との当たり判定（経路だけで避ける）。
    class AllyComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<AllyComponent>(*this); }
        void Update(GameContext& ctx) override;

        // ---- 追従の設定 ----
        float moveSpeed = 4.5f;         // プレイヤー（4.0）より少し速くして、離されても追いつけるようにする
        float followDistance = 3.0f;    // 追いかけている間、ここまで近づいたら止まる
        float resumeDistance = 4.5f;    // 止まっている間、ここより離れたら動き出す（止まる距離との差で、動く・止まるを繰り返さない）
        float repathInterval = 0.5f;    // 経路を計算し直す間隔（秒）

        // ---- 攻撃の設定（2-2。段階 3 で、ロールの武器の値に置き換える）----
        float attackRange = 25.0f;      // この距離（m。水平）より近い敵だけを狙う
        float attackDamage = 34.0f;     // 1発のダメージ（プレイヤーと同じ。連射が遅く、弾がずれるので、プレイヤーより少し弱い）
        float attackInterval = 0.8f;    // 撃つ間隔（秒）
        float aimSpread = 2.0f;         // 弾のずれ（度）。狙った向きから、この角度の円の中にばらける
        float aimHeight = 1.6f;         // 撃つ位置の高さ（足元から。プレイヤーの目の高さと同じ）
        float searchInterval = 0.3f;    // 狙う敵を選び直す間隔（秒）
        float reactionTime = 0.3f;      // 新しい敵を選んでから、最初に撃つまでの時間（秒。見つけた瞬間に撃たない）

        // アニメーショングラフに渡すパラメータの名前（Player / Opponent と同じ名前）
        static constexpr const char* kAnimSpeed = "Speed";      // Float  ：いまの移動の速さ（m/s）。止まっていれば 0
        static constexpr const char* kAnimDead = "Dead";        // Bool   ：戦闘不能（HP 0）。HP が戻るまで true のまま
        static constexpr const char* kAnimAttack = "Attack";    // Trigger：撃った

        // HP が 0 で、戦闘不能か（動かない。撃たない。敵に狙われない。プレイヤーの弾を止めない）
        bool IsIncapacitated() const { return incapacitated; }

        // 今狙っている敵（無効 = 狙っていない）
        EntityId GetTarget() const { return targetId; }

        // 最後に撃った弾の線（HUD で描くため。2-2b）。GetShotTime() が 0 より大きい間だけ表示する
        const DirectX::XMFLOAT3& GetShotFrom() const { return shotFrom; }
        const DirectX::XMFLOAT3& GetShotTo() const { return shotTo; }
        float GetShotTime() const { return shotTime; }

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
        float    attackTimer = 0.0f;                // 次に撃てるまでの残り時間
        std::uint32_t rng = 0;                      // 弾のずれに使う乱数の状態（0 = まだ種を決めていない）
        DirectX::XMFLOAT3 shotFrom{ 0.0f, 0.0f, 0.0f };    // 最後に撃った弾の線の始まり
        DirectX::XMFLOAT3 shotTo{ 0.0f, 0.0f, 0.0f };      // 最後に撃った弾の線の終わり（当たった所。外れたら射程の先）
        float    shotTime = 0.0f;                   // 弾の線を出しておく残り時間
    };

} // namespace uel
