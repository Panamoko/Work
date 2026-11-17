#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Scene/EntityId.h"
#include "Engine/Game/Weapon.h"
#include <DirectXMath.h>

namespace uel {
    class PlayerComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<PlayerComponent>(*this); }

        void Update(GameContext& ctx) override;
        const DirectX::XMFLOAT4X4& GetView() const { return view; }
        float GetHitMarker() const { return hitMarker; }   // ★
        bool  GetHeadshot() const { return lastHeadshot; }

        // 回復（プレイヤーが Support のときだけ。3-2）。右クリックを押している間、照準の線上の味方へ回復の線をつなぐ。
        // 回復している相手（無効 = 回復していない）と、線の終わり（相手の胴体の真ん中。HUD で描くため）
        bool IsHealing() const { return healTargetId.IsValid(); }
        EntityId GetHealTarget() const { return healTargetId; }
        const DirectX::XMFLOAT3& GetHealTo() const { return healTo; }

        // 武器（3-3）。HUD に出す値：マガジンの残り、装弾数、リロードの進み具合（0〜1。リロードしていなければマイナス）
        int   GetAmmo() const { return hudAmmo; }
        int   GetMagazineSize() const { return hudMagazine; }
        float GetReloadProgress() const { return hudReload; }

        // 蘇生（4-2）。HUD に出す値：近くに蘇生できる味方がいるか、今蘇生しているか、その進み具合（0〜1）
        bool  CanRevive() const { return reviveCandidateId.IsValid(); }
        bool  IsReviving() const { return reviveTargetId.IsValid(); }
        float GetReviveProgress() const { return hudReviveProgress; }

        float moveSpeed = 4.0f;
        float eyeHeight = 1.6f;
        float eyeForward = 0.5f;
        float groundY = 0.0f;
        float radius = 0.35f;

        // アニメーショングラフに渡すパラメータの名前。
        // 同じ Entity の AnimatorComponent のグラフに、この名前のパラメータがあれば値を書く（無ければ何もしない）。
        // Speed / Attack / Dead は、敵（OpponentComponent）と同じ名前にしてある。
        static constexpr const char* kAnimSpeed = "Speed";          // Float  ：いまの移動の速さ（m/s）。壁に止められていれば 0
        static constexpr const char* kAnimMoveX = "MoveX";          // Float  ：体の向きから見た横の動き（右 +1 / 左 -1）
        static constexpr const char* kAnimMoveY = "MoveY";          // Float  ：体の向きから見た前後の動き（前 +1 / 後ろ -1）
        static constexpr const char* kAnimGrounded = "Grounded";    // Bool   ：地面に立っている
        static constexpr const char* kAnimJump = "Jump";            // Trigger：跳んだ
        static constexpr const char* kAnimAttack = "Attack";        // Trigger：撃った
        static constexpr const char* kAnimDead = "Dead";            // Bool   ：HP が 0
        static constexpr const char* kAnimAimPitch = "AimPitch";    // Float  ：視点の上下の角度（度。上が +）。エイムオフセットが使う
        static constexpr const char* kAnimHeal = "Heal";            // Bool   ：回復の線をつないでいる（AllyComponent と同じ名前）
        static constexpr const char* kAnimDowned = "Downed";        // Bool   ：ダウン中（蘇生を待っている。Dead も同時に true。4-4）
        static constexpr const char* kAnimRevive = "Revive";        // Bool   ：仲間を起こしている（AllyComponent と同じ名前。4-4）
    private:
        float pitch = 0.0f;
        float velY = 0.0f;
        float hitMarker = 0.0f;   // ★命中演出タイマー
        bool  lastHeadshot = false;
        DirectX::XMFLOAT4X4 view{};

        // 回復（3-2。実行中の状態。保存しない）
        EntityId healTargetId;                          // 今回復している味方（無効 = 回復していない）
        float    healTraceTimer = 0.0f;                 // 次に照準の線上を調べ直すまでの残り時間
        bool     healAnim = false;                      // アニメに渡した Heal の値（変わったときだけ書く）
        DirectX::XMFLOAT3 healTo{ 0.0f, 0.0f, 0.0f };   // 回復の線の終わり（相手の胴体の真ん中）

        // 武器（3-3。実行中の状態。保存しない）
        WeaponState fallbackWeapon;     // ロールの無いときに使う武器の状態（ロールがあれば、ロールの Component の weaponState を使う）
        int   hudAmmo = 0;              // HUD に出す値（毎フレームの最後に覚える）
        int   hudMagazine = 0;
        float hudReload = -1.0f;

        // 蘇生（4-2。実行中の状態。保存しない）
        EntityId reviveCandidateId;     // 近くにいる、蘇生できるダウンした味方（無効 = いない）
        EntityId reviveTargetId;        // 今蘇生している味方（無効 = 蘇生していない）
        float    reviveScanTimer = 0.0f;    // 次に近くのダウンした味方を探し直すまでの残り時間
        float    hudReviveProgress = 0.0f;  // HUD に出す蘇生の進み具合（0〜1）
        float    downedCamBlend = 0.0f;     // ダウン中の低い視点への寄り具合（0 = 立っている、1 = 倒れている。4-4）
    };
}
