#pragma once
// アニメのノードエディタ（Anim Graph パネル）の内部用ヘッダ。
// 次の3つの .cpp からだけ読み込む（ほかのファイルからは使わない）。
//   AnimGraphPanel.cpp            … パネルの入口、上のアセットの行、左のパラメータの欄
//   AnimGraphCanvas.cpp           … 共有の状態 g の実体、描画の補助、ポーズグラフの表示（DrawPoseView）
//   AnimGraphStateMachineView.cpp … ステートマシンの表示（DrawStateMachineView）
//
// ノードエディタは3つの階層を行き来する（UE の Animation Blueprint と同じ分け方）。
//
//   AnimGraph        … 根のノード → Output Pose。ここで「何が最終の姿勢を出すか」を決める
//    └ State Machine … 状態（箱）と遷移（矢印）。根がステートマシンのとき、または根の LayeredBlend の入力がステートマシンのときに入れる
//       └ State      … その状態の中身のノード → Output Pose
//
// AnimGraph と State は「ノードの出力を Output Pose へつなぐ」同じ形なので、同じ関数（DrawPoseView）で描く。
//
// 座標は2種類ある。
//   グラフ上の位置 … 箱の位置（editorX / editorY）や大きさ（kNodeW など）。拡大縮小しても変わらない
//   画面上の位置   … 画面の点 = キャンバスの左上 + scroll + グラフ上の位置 × zoom

#include "Engine/Scene/EntityId.h"
#include "Engine/Animation/AnimStateMachineNode.h"
#include <imgui.h>
#include <string>

namespace uel {
    class AnimatorComponent;

    namespace animgraph {
        using SM = AnimStateMachineNode;

        const float kNodeW = 160.0f;    // 状態の箱の大きさ（グラフ上の長さ）
        const float kNodeH = 46.0f;
        const float kAnyW = 70.0f;      // 「Any」の箱
        const float kAnyH = 30.0f;
        const float kDetailsW = 330.0f; // 右の編集欄の幅
        const float kParamsW = 300.0f;  // 左のパラメータの欄の幅

        const float kPoseNodeW = 210.0f;    // ポーズグラフのノードの箱
        const float kPoseNodeH = 62.0f;
        const float kOutputW = 130.0f;      // 「Output Pose」の箱
        const float kOutputX = 330.0f;      // ノードの箱から見た、Output Pose の箱の位置

        const float kZoomMin = 0.35f;
        const float kZoomMax = 2.0f;

        // 選んでいるもの（ステートマシンの階層）
        enum class SelKind { None, State, Transition };

        // どの階層を見ているか
        enum class Level { AnimGraph, StateMachine, State };

        // つかんでいるもの（左ドラッグ中）
        const int kDragNothing = -3;    // 何も動かさない（矢印をクリックしたときなど）
        const int kDragPan = -2;        // 背景をつかんだ：画面を動かす
        const int kDragAny = -1;        // Any の箱。0 以上は状態の番号

        // 遷移を作っている途中かどうか（「ここから」の状態）
        const int kNoLink = -3;         // 作っていない。-1 = Any から、0 以上 = その状態から

        // パネルが覚えている状態（フレームをまたいで残す）
        struct PanelState {
            EntityId lastEntity;
            Level    level = Level::AnimGraph;
            int      levelState = -1;               // Level::State のとき、見ている状態の番号
            bool     fitRequested = false;          // 「全体が見えるように合わせる」を頼まれた（次に描く階層が受け取る）

            // ステートマシンの階層
            ImVec2   scroll = ImVec2(110.0f, 70.0f);    // グラフの原点が、キャンバスの左上からどれだけ離れているか（画面上の長さ）
            float    zoom = 1.0f;
            int      dragTarget = kDragNothing;
            bool     panning = false;
            ImVec2   rightDownPos = ImVec2(0.0f, 0.0f);
            SelKind  selKind = SelKind::None;
            int      selIndex = -1;
            int      linkFrom = kNoLink;
            SelKind  ctxKind = SelKind::None;       // 右クリックのメニューを開いた対象
            int      ctxIndex = -1;
            ImVec2   ctxGraphPos = ImVec2(0.0f, 0.0f);

            // ポーズグラフの階層（AnimGraph / State）
            ImVec2   poseScroll = ImVec2(90.0f, 90.0f);
            float    poseZoom = 1.0f;
            bool     posePanning = false;
            ImVec2   poseRightDownPos = ImVec2(0.0f, 0.0f);
            bool     poseSelected = false;          // ノードの箱を選んでいるか
            int      poseInput = -1;                // 選んでいるのが LayeredBlend の入力の箱なら 0（下地）か 1（重ねる側）。ノード本体なら -1
            int      smSlot = 0;                    // 開いているステートマシンの場所：0 = 根、1 = 根の LayeredBlend の下地、2 = 重ねる側

            // パラメータの名前を打っている途中の内容（打ち終わったときに、まとめて反映する）
            int      renameIndex = -1;
            char     renameBuf[64] = "";

            void ClearSelection() { selKind = SelKind::None; selIndex = -1; }
            void ResetForNewGraph() {
                ClearSelection(); linkFrom = kNoLink; dragTarget = kDragNothing;
                level = Level::AnimGraph; levelState = -1; poseSelected = false; poseInput = -1; smSlot = 0; renameIndex = -1;
                fitRequested = true;        // 新しいグラフは、全体が見えるように合わせて開く
            }
        };
        extern PanelState g;     // 実体は AnimGraphCanvas.cpp

        // ---- 描画の補助（AnimGraphCanvas.cpp）----
        void        AddTextZ(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float zoom);
        ImVec2      TextSizeZ(const char* text, float zoom);
        void        ApplyWheelZoom(ImVec2 origin, ImVec2& scroll, float& zoom);
        std::string TransitionLabel(const SM::Transition& tr, const AnimParams& params);
        std::string NodeSummary(AnimNode* n);
        bool        ArrowEnds(ImVec2 c0, ImVec2 c1, ImVec2 half0, ImVec2 half1, float offset, ImVec2& p0, ImVec2& p1);
        void        DrawArrow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 col, float thickness, const char* label, float zoom);
        float       DistToSegment(ImVec2 m, ImVec2 p0, ImVec2 p1);
        void        DrawCanvasBackground(ImDrawList* dl, ImVec2 origin, ImVec2 size, ImVec2 scroll, float zoom);
        AnimNodePtr MakeClipNode(const SkinnedModel* model);
        AnimNodePtr MakeStateMachineNode(const SkinnedModel* model);

        // ---- 各階層の表示 ----
        // ポーズグラフ（AnimGraph / State）。開くステートマシン（0 = ノード本体、1 = 下地、2 = 重ねる側）を返す。無ければ -1（AnimGraphCanvas.cpp）
        int  DrawPoseView(AnimatorComponent* an, AnimNodePtr& slot, bool allowStateMachine, const SkinnedModel* model);
        // ステートマシン。開く状態の番号を返す（無ければ -1）（AnimGraphStateMachineView.cpp）
        int  DrawStateMachineView(AnimatorComponent* an, SM* sm, const SkinnedModel* model);

    } // namespace animgraph
} // namespace uel
