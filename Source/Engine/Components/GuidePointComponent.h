#pragma once
#include "Engine/Components/Component.h"

namespace uel {

    // 案内ロボット（GuideComponent）が通る道の「経由点」（5-1）。UE の Target Point（またはスプラインの点）に当たる。
    // Scene に置いた Entity の Transform の位置が、その点の位置になる（見た目は無い。エディタでは道の線と番号で見える）。
    // ロボットは order の小さい順に、点から点へまっすぐ進む。stopHere の点に着くと止まり、プレイヤーの合図を待つ
    // （5-2 から、止まる場所でウェーブが始まる）。
    // 動きは無いので Update は持たない（ロボットが Play の始めに1回だけ読む）。
    class GuidePointComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<GuidePointComponent>(*this); }

        // ---- 設定（保存する。版 24）----
        int  order = 0;             // 通る順番（小さいほうが先。同じ番号は、Scene に置いた順）
        bool stopHere = false;      // ここで止まる（ウェーブの場所）
    };

} // namespace uel
