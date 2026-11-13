#pragma once
#include "Engine/Components/RoleComponent.h"

namespace uel {

    // Support：味方の HP を回復する。
    // ロールの Component は「何ができるか」（値）だけを持つ。「いつ・誰を回復するか」は操作する側が決める
    // （AI 仲間なら AllyComponent。プレイヤーの回復（右クリック）は段階 3・6 で PlayerComponent に足す）。
    //
    // 回復ビーム（2-3a）：Overwatch のイラリーのように、主の攻撃（左クリック）とは別の「回復の射撃」（右クリック）として持つ。
    //   線でつないでいる間、毎秒 healPerSecond ずつ回復する。回復しているときは撃たない（攻撃と回復は同時に使えない）。
    // まだ無いこと：回復のエネルギー（使うと減って、休むとたまる）、武器の値（段階 3）、アビリティ（段階 6）。
    // Play の始めに PartySetupComponent が付けるので、値は保存しない（ここの初期値が使われる）。
    class SupportComponent : public RoleComponent {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<SupportComponent>(*this); }
        Role GetRole() const override { return Role::Support; }

        float healPerSecond = 12.0f;    // 1 秒あたりの回復量（ユーザーが選んだ。HP 100 を約 8 秒で満タン）
        float healRange = 15.0f;        // 回復の線が届く距離（m。水平）
        float healStartRatio = 0.65f;   // HP の割合がこれ以下の味方がいたら回復を始める（ユーザーが選んだ）
        float healStopRatio = 1.0f;     // 回復している相手の HP の割合がこれ以上になったら止める（始める値と分けて、行ったり来たりしない）
    };

} // namespace uel
