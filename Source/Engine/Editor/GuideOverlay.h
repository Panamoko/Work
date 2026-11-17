#pragma once
#include "Engine/Scene/EntityId.h"
#include <DirectXMath.h>
#include <imgui.h>

namespace uel {
    class World; class Entity;

    // Viewport の画像の上に 3D の点・線を写すための情報（カメラの行列と、画像の位置・大きさ）
    struct OverlayView {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
        ImVec2 imgPos;
        float  width = 0.0f, height = 0.0f;
    };

    // 案内ロボットの表示（5-1）。ViewportPanel.cpp が長くなりすぎないように、別のファイルに分けた。
    //
    // 編集中：経由点（GuidePointComponent）を順番の線でつなぎ、番号を出す。止まる場所は大きい輪と「STOP n」。
    //         ロボット（GuideComponent）から最初の点へも線を引く。経由点は見た目を持たないので、これで位置が分かる
    void DrawGuideRoute(ImDrawList* dl, World& world, const OverlayView& v, EntityId selected);

    // Play 中：ロボットの頭の上に印（壁越しでも見え、画面の外なら方向の画面の端）と、状態・距離。
    //          近くで合図できるときは、照準の下に「F：ロボットを進める」（mouseCaptured のときだけ）
    void DrawGuideHud(ImDrawList* dl, World& world, const Entity& player, const OverlayView& v, bool mouseCaptured);

} // namespace uel
