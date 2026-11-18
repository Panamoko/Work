#pragma once
#include "Engine/Editor/GuideOverlay.h"     // OverlayView

namespace uel {
    class World;

    // ウェーブとボスの HUD（5-3 で ViewportPanel.cpp から移した。ViewportPanel.cpp が 900 行を超えないように）。
    // ・上の真ん中に「WAVE n / N    ENEMIES e」（案内ロボットがいれば止まる場所で数える。5-2）、Spawner のエラーは赤で
    // ・ウェーブが始まったときの大きな「WAVE n」
    // ・ボス（BossComponent ＋ HPComponent）がいる間、その下に名前と HP バー。現れたときは、名前を大きく 2.5 秒出す
    void DrawWaveHud(ImDrawList* dl, World& world, const OverlayView& v);

} // namespace uel
