#pragma once
#include "Engine/Components/Component.h"
#include <string>

namespace uel {

    // ボスの目印（5-3。段階 7 までの仮のボス）。
    // これを持つ敵（OpponentComponent ＋ HPComponent）は、HUD が画面の上にボスの HP バーを出す（Editor/WaveOverlay）。
    // 動き・攻撃は普通の敵と同じ OpponentComponent（大きさ・HP・攻撃の値を Prefab で変えて「大きくて硬い敵」にする）。
    // 段階 7 で、突進・特殊攻撃などのボスだけの行動（予備動作 → 攻撃 → 硬直）をこの部品に足していく。
    // UE で言うと、ボスのクラス（またはタグ）を HUD が探して、ボスの HP バーを出す作り。
    // 動きは無いので Update は持たない。
    class BossComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<BossComponent>(*this); }

        // ---- 設定（保存する。版 26）----
        std::string displayName = "BOSS";   // HP バーの上に出す名前（HUD のフォントにある文字だけ。漢字は範囲を確かめる）
    };

} // namespace uel
