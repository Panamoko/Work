#pragma once
#include "Engine/Editor/GuideOverlay.h"     // OverlayView

namespace uel {
    class World;
    struct GameFlow;

    // Title・ロール選択・Result の画面（ScreenUIComponent。5-5a で ViewportPanel.cpp から移した。ViewportPanel.cpp が 900 行を超えないように）。
    // ScreenUI を持つ最初の Entity の画面を1つだけ描く。ボタン（ImGui::Button）を置くので、Viewport のウィンドウの中で呼ぶ。
    // ・Title：題名・副題・ボタン1つ（Enter でも決定）
    // ・ロール選択（roleSelect）：3つのロールのボタン（1 / 2 / 3 キー、Enter は今のロール）
    // ・Result（showResult）：CLEAR / GAME OVER、ゲームオーバーの理由、ロール、タイム、どこまで進んだか（GameFlow の結果。GameRule が書く）
    void DrawScreenUI(ImDrawList* dl, World& world, GameFlow& flow, const OverlayView& v);

} // namespace uel
