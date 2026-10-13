#include "Engine/Editor/AnimGraphPanel.h"
#include "Engine/Editor/EditorPanels.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/AnimatorComponent.h"
#include "Engine/Scene/SkinnedMeshComponent.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Animation/AnimStateMachineNode.h"
#include "Engine/Animation/AnimBlendSpaceNode.h"
#include "Engine/Serialization/AnimGraphSerializer.h"

#include <imgui.h>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

// ノードエディタは3つの階層を行き来する（UE の Animation Blueprint と同じ分け方）。
//
//   AnimGraph        … 根のノード → Output Pose。ここで「何が最終の姿勢を出すか」を決める
//    └ State Machine … 状態（箱）と遷移（矢印）。根がステートマシンのときだけ入れる
//       └ State      … その状態の中身のノード → Output Pose
//
// AnimGraph と State は「ノードの出力を Output Pose へつなぐ」同じ形なので、同じ関数（DrawPoseView）で描く。
//
// 座標は2種類ある。
//   グラフ上の位置 … 箱の位置（editorX / editorY）や大きさ（kNodeW など）。拡大縮小しても変わらない
//   画面上の位置   … 画面の点 = キャンバスの左上 + scroll + グラフ上の位置 × zoom

namespace uel {
    namespace {
        using SM = AnimStateMachineNode;

        const float kNodeW = 160.0f;    // 状態の箱の大きさ（グラフ上の長さ）
        const float kNodeH = 46.0f;
        const float kAnyW = 70.0f;      // 「Any」の箱
        const float kAnyH = 30.0f;
        const float kDetailsW = 330.0f; // 右の編集欄の幅
        const float kParamsW = 250.0f;  // 左のパラメータの欄の幅

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

            // パラメータの名前を打っている途中の内容（打ち終わったときに、まとめて反映する）
            int      renameIndex = -1;
            char     renameBuf[64] = "";

            void ClearSelection() { selKind = SelKind::None; selIndex = -1; }
            void ResetForNewGraph() {
                ClearSelection(); linkFrom = kNoLink; dragTarget = kDragNothing;
                level = Level::AnimGraph; levelState = -1; poseSelected = false; renameIndex = -1;
            }
        };
        PanelState g;

        // ---- 拡大率に合わせた文字 ----
        void AddTextZ(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, float zoom) {
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * zoom, pos, col, text);
        }
        ImVec2 TextSizeZ(const char* text, float zoom) {
            return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * zoom, FLT_MAX, 0.0f, text);
        }

        // マウスホイールで拡大縮小する。マウスの下にある点が動かないように、scroll も直す。
        void ApplyWheelZoom(ImVec2 origin, ImVec2& scroll, float& zoom)
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseWheel == 0.0f) return;
            float nz = zoom * ((io.MouseWheel > 0.0f) ? 1.1f : (1.0f / 1.1f));
            if (nz < kZoomMin) nz = kZoomMin;
            if (nz > kZoomMax) nz = kZoomMax;
            // マウスの下の「グラフ上の位置」を求めて、新しい拡大率でも同じ画面の点に来るようにする
            const float gx = (io.MousePos.x - origin.x - scroll.x) / zoom;
            const float gy = (io.MousePos.y - origin.y - scroll.y) / zoom;
            scroll.x = io.MousePos.x - origin.x - gx * nz;
            scroll.y = io.MousePos.y - origin.y - gy * nz;
            zoom = nz;
        }

        // 遷移の条件を、矢印の横に出す短い文にする（例： "Speed > 0.10"、"Attack"、"at end"）
        std::string TransitionLabel(const SM::Transition& tr, const AnimParams& params)
        {
            std::string s;
            for (const SM::Condition& c : tr.conditions) {
                if (!s.empty()) s += " & ";
                const int pi = params.Find(c.param);
                if (pi < 0) { s += "?"; continue; }
                const AnimParam& p = params.items[pi];
                char buf[96];
                if (p.type == AnimParamType::Float)
                    snprintf(buf, sizeof(buf), "%s %s %.2f", c.param.c_str(), (c.op == SM::CondOp::Less) ? "<" : ">", c.threshold);
                else if (p.type == AnimParamType::Bool)
                    snprintf(buf, sizeof(buf), "%s%s", (c.op == SM::CondOp::IsFalse) ? "!" : "", c.param.c_str());
                else
                    snprintf(buf, sizeof(buf), "%s", c.param.c_str());
                s += buf;
            }
            if (tr.waitForEnd) s += s.empty() ? "at end" : " & at end";
            if (s.empty()) s = "(no condition)";
            return s;
        }

        // ノードの中身を1行で（例： "Clip: walk"、"BlendSpace1D (3)"、"StateMachine (3 states)"）
        std::string NodeSummary(AnimNode* n)
        {
            if (!n) return "(empty)";
            if (auto* cn = dynamic_cast<AnimClipNode*>(n)) return "Clip: " + cn->clipName;
            if (auto* bs = dynamic_cast<AnimBlendSpace1DNode*>(n)) return "BlendSpace1D (" + std::to_string(bs->samples.size()) + ")";
            if (auto* sm = dynamic_cast<SM*>(n)) return "StateMachine (" + std::to_string(sm->states.size()) + " states)";
            return n->TypeName();
        }

        // パラメータの名前を変えたとき、その名前を使っている所（条件、Blend Space の軸、再生速度）も書き換える
        void RenameParamRefs(AnimNode* n, const std::string& oldName, const std::string& newName)
        {
            if (!n) return;
            if (auto* cn = dynamic_cast<AnimClipNode*>(n)) {
                if (cn->speedParam == oldName) cn->speedParam = newName;
            }
            else if (auto* bs = dynamic_cast<AnimBlendSpace1DNode*>(n)) {
                if (bs->param == oldName) bs->param = newName;
            }
            else if (auto* sm = dynamic_cast<SM*>(n)) {
                for (SM::Transition& tr : sm->transitions)
                    for (SM::Condition& c : tr.conditions) if (c.param == oldName) c.param = newName;
                for (SM::State& st : sm->states) RenameParamRefs(st.node.get(), oldName, newName);
            }
        }

        // 箱 A の中心から箱 B の中心へ引く矢印の、両端の位置を求める（線の両端は箱のふちで止める）。
        //   half0 / half1 … それぞれの箱の「幅の半分・高さの半分」
        //   offset       … 線を直角な方向へずらす量（同じ2つの箱の間に何本もあるとき、重ならないようにするため）
        // 箱どうしが重なっていて線が引けないときは false。
        bool ArrowEnds(ImVec2 c0, ImVec2 c1, ImVec2 half0, ImVec2 half1, float offset, ImVec2& p0, ImVec2& p1)
        {
            float dx = c1.x - c0.x, dy = c1.y - c0.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0f) return false;
            dx /= len; dy /= len;
            const float px = -dy, py = dx;                          // 線に直角な向き
            c0.x += px * offset; c0.y += py * offset;
            c1.x += px * offset; c1.y += py * offset;

            // 中心から箱のふちまでの距離（向きによって変わる）
            auto edge = [&](ImVec2 h) {
                const float tx = (std::fabs(dx) > 1e-4f) ? h.x / std::fabs(dx) : 1e9f;
                const float ty = (std::fabs(dy) > 1e-4f) ? h.y / std::fabs(dy) : 1e9f;
                return (tx < ty) ? tx : ty;
                };
            const float t0 = edge(half0), t1 = edge(half1);
            if (t0 + t1 >= len) return false;
            p0 = ImVec2(c0.x + dx * t0, c0.y + dy * t0);
            p1 = ImVec2(c1.x - dx * t1, c1.y - dy * t1);
            return true;
        }

        // p0 から p1 へ、矢じりと説明の文が付いた線を描く
        void DrawArrow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 col, float thickness, const char* label, float zoom)
        {
            float dx = p1.x - p0.x, dy = p1.y - p0.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0f) return;
            dx /= len; dy /= len;
            const float px = -dy, py = dx;

            dl->AddLine(p0, p1, col, thickness);
            const float a = 10.0f * zoom, w = 5.0f * zoom;          // 矢じりの長さと幅
            dl->AddTriangleFilled(p1,
                ImVec2(p1.x - dx * a + px * w, p1.y - dy * a + py * w),
                ImVec2(p1.x - dx * a - px * w, p1.y - dy * a - py * w), col);

            if (label && label[0] && zoom >= 0.6f) {                // 小さくしすぎたら、文は出さない（読めないので）
                const ImVec2 ts = TextSizeZ(label, zoom);
                const ImVec2 mid((p0.x + p1.x) * 0.5f + px * 10.0f * zoom, (p0.y + p1.y) * 0.5f + py * 10.0f * zoom);
                const ImVec2 tp(mid.x - ts.x * 0.5f, mid.y - ts.y * 0.5f);
                dl->AddRectFilled(ImVec2(tp.x - 3, tp.y - 1), ImVec2(tp.x + ts.x + 3, tp.y + ts.y + 1), IM_COL32(24, 26, 31, 220), 3.0f);
                AddTextZ(dl, tp, col, label, zoom);
            }
        }

        // 点 m から、線分 p0-p1 までの距離（矢印をクリックしたかどうかの判定に使う）
        float DistToSegment(ImVec2 m, ImVec2 p0, ImVec2 p1)
        {
            const float vx = p1.x - p0.x, vy = p1.y - p0.y;
            const float l2 = vx * vx + vy * vy;
            float t = (l2 > 1e-6f) ? ((m.x - p0.x) * vx + (m.y - p0.y) * vy) / l2 : 0.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            const float qx = p0.x + vx * t - m.x, qy = p0.y + vy * t - m.y;
            return std::sqrt(qx * qx + qy * qy);
        }

        // キャンバスの背景（地の色と、格子の線）
        void DrawCanvasBackground(ImDrawList* dl, ImVec2 origin, ImVec2 size, ImVec2 scroll, float zoom)
        {
            const ImVec2 cmax(origin.x + size.x, origin.y + size.y);
            dl->AddRectFilled(origin, cmax, IM_COL32(24, 26, 31, 255));
            const float grid = 32.0f * zoom;
            if (grid < 8.0f) return;        // 細かくなりすぎたら、線は引かない
            float x0 = std::fmod(scroll.x, grid); if (x0 < 0.0f) x0 += grid;
            float y0 = std::fmod(scroll.y, grid); if (y0 < 0.0f) y0 += grid;
            for (float x = x0; x < size.x; x += grid)
                dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, cmax.y), IM_COL32(40, 43, 50, 255));
            for (float y = y0; y < size.y; y += grid)
                dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(cmax.x, origin.y + y), IM_COL32(40, 43, 50, 255));
        }

        // 新しいノードを作る
        AnimNodePtr MakeClipNode(const SkinnedModel* model)
        {
            auto n = std::make_unique<AnimClipNode>();
            if (model && !model->clips.empty()) n->clipName = model->clips[0].name;    // モデルが分かっていれば、最初のクリップを選んでおく
            return AnimNodePtr(std::move(n));
        }
        AnimNodePtr MakeStateMachineNode(const SkinnedModel* model)
        {
            auto n = std::make_unique<SM>();    // 状態を1つだけ持つ形で始める
            SM::State st;
            st.name = "Idle";
            st.node = MakeClipNode(model);
            n->states.push_back(std::move(st));
            return AnimNodePtr(std::move(n));
        }

        // =====================================================================
        //  上の行：グラフのアセット（保存・読み込み）。読み込んだら true（ノードが作り直された）
        // =====================================================================
        bool DrawAssetRow(AnimatorComponent* an)
        {
            static std::string graphMsg;
            static float       graphMsgTimer = 0.0f;
            bool reloaded = false;

            char gb[200]; strncpy_s(gb, sizeof(gb), an->graphPath.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(340.0f);
            if (ImGui::InputText("Graph Asset", gb, sizeof(gb))) an->graphPath = gb;       // 名前だけでもよい（例：Enemy）
            if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の [Graph] をここへドロップすると読み込む
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_ANIMGRAPH")) {
                    if (pl->Data) {
                        const std::string path = static_cast<const char*>(pl->Data);
                        const bool ok = LoadAnimGraph(path, an->params, an->graph);
                        if (ok) { an->graphPath = path; reloaded = true; }
                        graphMsg = ok ? "Loaded" : "Load failed (file?)";
                        graphMsgTimer = 3.0f;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                const std::string path = AnimGraphPathFromName(an->graphPath);
                const bool ok = an->graph && !path.empty() && SaveAnimGraph(path, an->params, an->graph.get());
                if (ok) { an->graphPath = path; RequestAssetRescan(); }
                graphMsg = ok ? "Saved" : (an->graph ? "Save failed (name?)" : "No graph to save");
                graphMsgTimer = 3.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                const std::string path = AnimGraphPathFromName(an->graphPath);
                const bool ok = !path.empty() && LoadAnimGraph(path, an->params, an->graph);
                if (ok) { an->graphPath = path; reloaded = true; }
                graphMsg = ok ? "Loaded" : "Load failed (file?)";
                graphMsgTimer = 3.0f;
            }
            ImGui::SameLine();
            if (graphMsgTimer > 0.0f) {
                graphMsgTimer -= ImGui::GetIO().DeltaTime;
                ImGui::TextUnformatted(graphMsg.c_str());
            }
            else ImGui::TextDisabled("(edits are kept only after Save)");
            return reloaded;
        }

        // =====================================================================
        //  左の欄：パラメータ（グラフに渡す値）
        // =====================================================================
        void DrawParamsPane(AnimatorComponent* an)
        {
            ImGui::BeginChild("ag_params", ImVec2(kParamsW, 0.0f), true);
            ImGui::TextUnformatted("Parameters");
            ImGui::Separator();
            const char* typeNames[] = { "Float", "Bool", "Trigger" };   // same order as AnimParamType
            int removeAt = -1;
            if (g.renameIndex >= (int)an->params.items.size()) g.renameIndex = -1;
            for (int i = 0; i < (int)an->params.items.size(); ++i) {
                AnimParam& p = an->params.items[i];
                ImGui::PushID(i);
                int ty = (int)p.type;
                ImGui::SetNextItemWidth(66);
                if (ImGui::Combo("##type", &ty, typeNames, 3)) { p.type = (AnimParamType)ty; p.value = 0.0f; }
                ImGui::SameLine();

                // 名前：打っている途中は g.renameBuf に入れておき、打ち終わったとき（Enter か、欄の外をクリック）に反映する。
                // 1文字ごとに反映すると、その名前を使っている条件などが、途中の名前を追いかけきれないため。
                char nb[64]; strncpy_s(nb, sizeof(nb), p.name.c_str(), _TRUNCATE);
                char* buf = (g.renameIndex == i) ? g.renameBuf : nb;
                ImGui::SetNextItemWidth(78);
                ImGui::InputText("##name", buf, sizeof(nb));
                if (ImGui::IsItemActivated()) { g.renameIndex = i; strncpy_s(g.renameBuf, sizeof(g.renameBuf), p.name.c_str(), _TRUNCATE); }
                if (g.renameIndex == i && ImGui::IsItemDeactivated()) {
                    const std::string oldName = p.name, newName = g.renameBuf;
                    bool taken = false;     // 空の名前と、ほかのパラメータと同じ名前は受け付けない
                    for (int k = 0; k < (int)an->params.items.size(); ++k) if (k != i && an->params.items[k].name == newName) taken = true;
                    if (!newName.empty() && !taken && newName != oldName) {
                        p.name = newName;
                        RenameParamRefs(an->graph.get(), oldName, newName);     // 条件や Blend Space の軸も、新しい名前に付け替える
                    }
                    g.renameIndex = -1;
                }

                ImGui::SameLine();
                if (p.type == AnimParamType::Float) {
                    ImGui::SetNextItemWidth(52);
                    ImGui::DragFloat("##val", &p.value, 0.01f, 0.0f, 0.0f, "%.2f");
                }
                else if (p.type == AnimParamType::Bool) {
                    bool b = p.value > 0.5f;
                    if (ImGui::Checkbox("##val", &b)) p.value = b ? 1.0f : 0.0f;
                }
                else {
                    if (ImGui::SmallButton("Fire")) p.value = 1.0f;     // このフレームだけ立つ
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) removeAt = i;
                ImGui::PopID();
            }
            if (removeAt >= 0) { an->params.items.erase(an->params.items.begin() + removeAt); g.renameIndex = -1; }
            if (ImGui::SmallButton("+ Parameter")) {
                // ほかと重ならない名前を付ける
                std::string name;
                for (int n = (int)an->params.items.size(); ; ++n) {
                    name = "Param" + std::to_string(n);
                    if (an->params.Find(name) < 0) break;
                }
                AnimParam p; p.name = name;
                an->params.items.push_back(p);
            }
            ImGui::Spacing();
            ImGui::TextDisabled("Float : compare with > or <");
            ImGui::TextDisabled("Bool  : true / false");
            ImGui::TextDisabled("Trigger : fires once");
            ImGui::Spacing();
            if (!an->lastEvent.empty() && an->lastEventAge < 1.0f) ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.35f, 1.0f), "Notify: %s", an->lastEvent.c_str());
            else ImGui::TextDisabled("Notify: -");
            ImGui::EndChild();
        }

        // =====================================================================
        //  ポーズグラフの階層（AnimGraph / State）： ノード → Output Pose
        //    slot             … 編集するノードの入れ物（AnimGraph なら根、State ならその状態の中身）
        //    allowStateMachine … ステートマシンを置けるか（根だけ）
        //  ステートマシンのノードをダブルクリックしたら true（呼び出し側が、中の階層へ移る）
        // =====================================================================
        bool DrawPoseView(AnimatorComponent* an, AnimNodePtr& slot, bool allowStateMachine, const SkinnedModel* model)
        {
            ImGuiIO& io = ImGui::GetIO();
            bool enter = false;

            // ---------------- 中央：キャンバス ----------------
            ImGui::BeginChild("ag_pose_canvas", ImVec2(-(kDetailsW + 8.0f), 0.0f), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                ImVec2 origin = ImGui::GetCursorScreenPos();
                ImVec2 size = ImGui::GetContentRegionAvail();
                if (size.x < 60.0f) size.x = 60.0f;
                if (size.y < 60.0f) size.y = 60.0f;
                ImGui::InvisibleButton("pose_canvas", size);
                const bool hovered = ImGui::IsItemHovered();
                const bool active = ImGui::IsItemActive();
                const bool activated = ImGui::IsItemActivated();

                // 「合わせる」：最初の見え方に戻す。ホイール：拡大縮小
                if (g.fitRequested) { g.fitRequested = false; g.poseScroll = ImVec2(90.0f, 90.0f); g.poseZoom = 1.0f; }
                if (hovered) ApplyWheelZoom(origin, g.poseScroll, g.poseZoom);
                const float z = g.poseZoom;

                // 箱の位置は固定（ノードが左、Output Pose が右）。動かせるのは画面だけ
                const ImVec2 nodeMin(origin.x + g.poseScroll.x, origin.y + g.poseScroll.y);
                const ImVec2 nodeMax(nodeMin.x + kPoseNodeW * z, nodeMin.y + kPoseNodeH * z);
                const ImVec2 outMin(nodeMin.x + kOutputX * z, nodeMin.y);
                const ImVec2 outMax(outMin.x + kOutputW * z, outMin.y + kPoseNodeH * z);
                const bool hasNode = (bool)slot;
                const bool overNode = hasNode && io.MousePos.x >= nodeMin.x && io.MousePos.x <= nodeMax.x
                    && io.MousePos.y >= nodeMin.y && io.MousePos.y <= nodeMax.y;

                // 入力：右ドラッグで画面の移動、動かさずに離せばメニュー
                if (hovered && ImGui::IsMouseClicked(1)) { g.posePanning = true; g.poseRightDownPos = io.MousePos; }
                if (hovered && ImGui::IsMouseClicked(2)) g.posePanning = true;
                if (g.posePanning) {
                    if (io.MouseDown[1] || io.MouseDown[2]) { g.poseScroll.x += io.MouseDelta.x; g.poseScroll.y += io.MouseDelta.y; }
                    else g.posePanning = false;
                }
                if (hovered && ImGui::IsMouseReleased(1)) {
                    const float mdx = io.MousePos.x - g.poseRightDownPos.x, mdy = io.MousePos.y - g.poseRightDownPos.y;
                    if (mdx * mdx + mdy * mdy < 16.0f) { if (overNode) g.poseSelected = true; ImGui::OpenPopup("ag_pose_ctx"); }
                }
                if (activated) g.poseSelected = overNode;
                if (active && !g.poseSelected) { g.poseScroll.x += io.MouseDelta.x; g.poseScroll.y += io.MouseDelta.y; }     // 背景をつかんだ：画面を動かす
                if (hovered && overNode && ImGui::IsMouseDoubleClicked(0) && dynamic_cast<SM*>(slot.get())) enter = true;
                if (hovered && !io.WantTextInput && ImGui::IsKeyPressed('F')) g.fitRequested = true;

                // 右クリックのメニュー：ノードを置く／置き換える／消す
                if (ImGui::BeginPopup("ag_pose_ctx")) {
                    ImGui::TextDisabled(hasNode ? "Replace the node with" : "Add a node");
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clip")) { slot = MakeClipNode(model); g.poseSelected = true; }
                    if (ImGui::MenuItem("Blend Space 1D")) { slot = AnimNodePtr(std::make_unique<AnimBlendSpace1DNode>()); g.poseSelected = true; }
                    if (allowStateMachine && ImGui::MenuItem("State Machine")) { slot = MakeStateMachineNode(model); g.poseSelected = true; }
                    if (hasNode) {
                        ImGui::Separator();
                        if (ImGui::MenuItem("Delete node")) { slot.reset(); g.poseSelected = false; }
                    }
                    ImGui::EndPopup();
                }

                // 描画
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
                DrawCanvasBackground(dl, origin, size, g.poseScroll, z);

                const ImU32 colWire = IM_COL32(120, 200, 255, 255);
                const ImVec2 outPin(outMin.x, outMin.y + kPoseNodeH * z * 0.5f);        // Output Pose の入力ピン（左の辺の中央）

                // Output Pose の箱
                dl->AddRectFilled(outMin, outMax, IM_COL32(60, 50, 44, 255), 6.0f * z);
                dl->AddRect(outMin, outMax, IM_COL32(210, 160, 110, 255), 6.0f * z);
                AddTextZ(dl, ImVec2(outMin.x + 16 * z, outMin.y + 8 * z), IM_COL32(235, 238, 245, 255), "Output Pose", z);
                AddTextZ(dl, ImVec2(outMin.x + 16 * z, outMin.y + 30 * z), IM_COL32(150, 156, 170, 255), "Result", z);
                dl->AddCircleFilled(outPin, 5.0f * z, slot ? colWire : IM_COL32(90, 94, 104, 255));

                if (slot) {
                    AnimNode* n = slot.get();
                    const ImVec2 nodePin(nodeMax.x, nodeMin.y + kPoseNodeH * z * 0.5f); // ノードの出力ピン（右の辺の中央）

                    // 線（ノードの出力 → Output Pose の入力）。横に出てから曲がる形にする
                    dl->AddBezierCurve(nodePin, ImVec2(nodePin.x + 60 * z, nodePin.y), ImVec2(outPin.x - 60 * z, outPin.y), outPin, colWire, 2.5f * z);

                    // ノードの箱
                    const bool isSM = dynamic_cast<SM*>(n) != nullptr;
                    dl->AddRectFilled(nodeMin, nodeMax, isSM ? IM_COL32(46, 58, 82, 255) : IM_COL32(52, 56, 66, 255), 6.0f * z);
                    dl->AddRect(nodeMin, nodeMax, IM_COL32(110, 116, 130, 255), 6.0f * z);
                    if (g.poseSelected)
                        dl->AddRect(ImVec2(nodeMin.x - 3, nodeMin.y - 3), ImVec2(nodeMax.x + 3, nodeMax.y + 3), IM_COL32(255, 255, 255, 255), 8.0f * z, ImDrawCornerFlags_All, 1.5f);
                    AddTextZ(dl, ImVec2(nodeMin.x + 10 * z, nodeMin.y + 8 * z), IM_COL32(235, 238, 245, 255), n->TypeName(), z);
                    const std::string sub = isSM ? (NodeSummary(n) + "  (double-click)") : NodeSummary(n);
                    dl->PushClipRect(nodeMin, ImVec2(nodeMax.x - 8 * z, nodeMax.y), true);
                    AddTextZ(dl, ImVec2(nodeMin.x + 10 * z, nodeMin.y + 32 * z), IM_COL32(150, 156, 170, 255), sub.c_str(), z);
                    dl->PopClipRect();
                    dl->AddCircleFilled(nodePin, 5.0f * z, colWire);
                }
                else {
                    AddTextZ(dl, ImVec2(nodeMin.x, nodeMin.y + 20 * z), IM_COL32(150, 156, 170, 255), "Right-click: add a node", z);
                }
                dl->PopClipRect();
            }
            ImGui::EndChild();

            // ---------------- 右：ノードの編集欄 ----------------
            ImGui::SameLine();
            ImGui::BeginChild("ag_pose_details", ImVec2(0.0f, 0.0f), true);
            {
                AnimNode* n = slot.get();
                if (n && g.poseSelected) {
                    ImGui::Text("%s node", n->TypeName());
                    ImGui::Separator();
                    if (auto* sm = dynamic_cast<SM*>(n)) {
                        ImGui::Text("%d states / %d transitions", (int)sm->states.size(), (int)sm->transitions.size());
                        if (ImGui::Button("Open State Machine")) enter = true;
                    }
                    else if (model) DrawAnimNodeDetails(n, *model, an->params);     // クリップや Blend Space の中身
                    else ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "No model: set a Skinned GLB to pick clips.");
                }
                else {
                    ImGui::TextUnformatted(n ? "Nothing selected" : "No node");
                    ImGui::Separator();
                    ImGui::TextDisabled("The node on the left feeds Output Pose.");
                    ImGui::TextDisabled("Click the node to edit it.");
                    ImGui::TextDisabled("Right-click: add / replace / delete.");
                    if (allowStateMachine) ImGui::TextDisabled("Double-click a State Machine to open it.");
                }
            }
            ImGui::EndChild();
            return enter;
        }

        // =====================================================================
        //  ステートマシンの階層：状態（箱）と遷移（矢印）
        //  状態をダブルクリックした（または Open を押した）ら、その番号を返す（無ければ -1）
        // =====================================================================
        int DrawStateMachineView(AnimatorComponent* an, SM* sm, const SkinnedModel* model)
        {
            ImGuiIO& io = ImGui::GetIO();
            int openState = -1;

            // 番号が外れていたら選択を外す
            if (g.selKind == SelKind::State && (g.selIndex < 0 || g.selIndex >= (int)sm->states.size())) g.ClearSelection();
            if (g.selKind == SelKind::Transition && (g.selIndex < 0 || g.selIndex >= (int)sm->transitions.size())) g.ClearSelection();
            if (g.linkFrom >= (int)sm->states.size()) g.linkFrom = kNoLink;

            // 最初の並べ方：どの状態もまだ置かれていなければ、3列に並べる
            {
                bool placed = false;
                for (const SM::State& st : sm->states) if (st.editorX != 0.0f || st.editorY != 0.0f) { placed = true; break; }
                if (!placed)
                    for (int i = 0; i < (int)sm->states.size(); ++i) {
                        sm->states[i].editorX = (float)(i % 3) * 230.0f;
                        sm->states[i].editorY = (float)(i / 3) * 120.0f;
                    }
            }

            // ---------------- 中央：キャンバス ----------------
            ImGui::BeginChild("ag_canvas", ImVec2(-(kDetailsW + 8.0f), 0.0f), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                ImVec2 origin = ImGui::GetCursorScreenPos();
                ImVec2 size = ImGui::GetContentRegionAvail();
                if (size.x < 60.0f) size.x = 60.0f;
                if (size.y < 60.0f) size.y = 60.0f;
                ImGui::InvisibleButton("canvas", size);     // マウスを受け取る部品。箱や矢印は、この上に線で描くだけ
                const bool hovered = ImGui::IsItemHovered();
                const bool active = ImGui::IsItemActive();
                const bool activated = ImGui::IsItemActivated();

                // 「Any」の箱は、Any からの遷移があるとき（または、いま作っている最中）だけ出す
                auto hasAnyBox = [&]() {
                    if (g.linkFrom == -1) return true;
                    for (const SM::Transition& tr : sm->transitions) if (tr.from == -1) return true;
                    return false;
                    };

                // 「合わせる」：すべての箱が入るように、拡大率と位置を決める
                if (g.fitRequested) {
                    g.fitRequested = false;
                    if (!sm->states.empty()) {
                        float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
                        auto grow = [&](float x, float y, float w, float h) {
                            if (x < mnx) mnx = x; if (y < mny) mny = y;
                            if (x + w > mxx) mxx = x + w; if (y + h > mxy) mxy = y + h;
                            };
                        for (const SM::State& st : sm->states) grow(st.editorX, st.editorY, kNodeW, kNodeH);
                        if (hasAnyBox()) grow(sm->anyX, sm->anyY, kAnyW, kAnyH);
                        const float bw = (mxx - mnx) + 200.0f, bh = (mxy - mny) + 100.0f;      // まわりに余白（Entry の文字ぶんも）
                        float nz = (size.x / bw < size.y / bh) ? size.x / bw : size.y / bh;
                        if (nz > 1.0f) nz = 1.0f;       // 等倍より大きくはしない
                        if (nz < kZoomMin) nz = kZoomMin;
                        g.zoom = nz;
                        g.scroll.x = (size.x - (mxx - mnx) * nz) * 0.5f - mnx * nz;
                        g.scroll.y = (size.y - (mxy - mny) * nz) * 0.5f - mny * nz;
                    }
                }
                if (hovered) ApplyWheelZoom(origin, g.scroll, g.zoom);
                const float z = g.zoom;
                const ImVec2 nodeSize(kNodeW * z, kNodeH * z);      // 画面上の箱の大きさ
                const ImVec2 anySize(kAnyW * z, kAnyH * z);
                const ImVec2 halfNode(nodeSize.x * 0.5f, nodeSize.y * 0.5f);
                const ImVec2 halfAny(anySize.x * 0.5f, anySize.y * 0.5f);

                // グラフ上の位置 → 画面上の位置
                auto toScreen = [&](float x, float y) { return ImVec2(origin.x + g.scroll.x + x * z, origin.y + g.scroll.y + y * z); };
                auto inRect = [&](ImVec2 mn, ImVec2 sz) {
                    return io.MousePos.x >= mn.x && io.MousePos.x <= mn.x + sz.x && io.MousePos.y >= mn.y && io.MousePos.y <= mn.y + sz.y;
                    };
                auto stateCenter = [&](int i) {
                    const ImVec2 p = toScreen(sm->states[i].editorX, sm->states[i].editorY);
                    return ImVec2(p.x + halfNode.x, p.y + halfNode.y);
                    };
                auto anyCenter = [&]() {
                    const ImVec2 p = toScreen(sm->anyX, sm->anyY);
                    return ImVec2(p.x + halfAny.x, p.y + halfAny.y);
                    };
                // 遷移の矢印の両端（描画と、クリックの判定の両方で使う）
                auto transitionEnds = [&](int index, ImVec2& p0, ImVec2& p1) {
                    const SM::Transition& tr = sm->transitions[index];
                    const int n = (int)sm->states.size();
                    if (tr.to < 0 || tr.to >= n || tr.from >= n) return false;
                    // 同じ向きの遷移が何本もあるときは、順番に横へずらす。逆向きがあるときも、重ならないようにずらす
                    int same = 0; bool reverse = false;
                    for (int k = 0; k < (int)sm->transitions.size(); ++k) {
                        const SM::Transition& o = sm->transitions[k];
                        if (k < index && o.from == tr.from && o.to == tr.to) ++same;
                        if (o.from == tr.to && o.to == tr.from) reverse = true;
                    }
                    const float offset = ((reverse ? 9.0f : 0.0f) + 16.0f * (float)same) * z;
                    if (tr.from == -1) return ArrowEnds(anyCenter(), stateCenter(tr.to), halfAny, halfNode, offset, p0, p1);
                    return ArrowEnds(stateCenter(tr.from), stateCenter(tr.to), halfNode, halfNode, offset, p0, p1);
                    };
                // マウスの下にあるもの（後に描いた箱が手前なので、後ろから調べる）
                auto hitState = [&]() {
                    for (int i = (int)sm->states.size() - 1; i >= 0; --i)
                        if (inRect(toScreen(sm->states[i].editorX, sm->states[i].editorY), nodeSize)) return i;
                    return -1;
                    };
                auto hitTransition = [&]() {
                    for (int i = (int)sm->transitions.size() - 1; i >= 0; --i) {
                        ImVec2 p0, p1;
                        if (transitionEnds(i, p0, p1) && DistToSegment(io.MousePos, p0, p1) < 7.0f) return i;
                    }
                    return -1;
                    };

                // ---- 入力 ----
                // 右ボタン：動かせば画面の移動、動かさずに離せばメニュー（遷移を作っている途中なら取り消し）
                if (hovered && ImGui::IsMouseClicked(1)) { g.panning = true; g.rightDownPos = io.MousePos; }
                if (hovered && ImGui::IsMouseClicked(2)) g.panning = true;
                if (g.panning) {
                    if (io.MouseDown[1] || io.MouseDown[2]) { g.scroll.x += io.MouseDelta.x; g.scroll.y += io.MouseDelta.y; }
                    else g.panning = false;
                }
                if (hovered && ImGui::IsMouseReleased(1)) {
                    const float mdx = io.MousePos.x - g.rightDownPos.x, mdy = io.MousePos.y - g.rightDownPos.y;
                    if (mdx * mdx + mdy * mdy < 16.0f) {
                        if (g.linkFrom != kNoLink) g.linkFrom = kNoLink;
                        else {
                            const int hs = hitState();
                            const int ht = (hs < 0) ? hitTransition() : -1;
                            if (hs >= 0) { g.ctxKind = SelKind::State; g.ctxIndex = hs; g.selKind = SelKind::State; g.selIndex = hs; }
                            else if (ht >= 0) { g.ctxKind = SelKind::Transition; g.ctxIndex = ht; g.selKind = SelKind::Transition; g.selIndex = ht; }
                            else { g.ctxKind = SelKind::None; g.ctxIndex = -1; }
                            g.ctxGraphPos = ImVec2((io.MousePos.x - origin.x - g.scroll.x) / z, (io.MousePos.y - origin.y - g.scroll.y) / z);
                            ImGui::OpenPopup("ag_ctx");
                        }
                    }
                }
                if (g.linkFrom != kNoLink && ImGui::IsKeyPressed(0x1B)) g.linkFrom = kNoLink;      // Esc
                if (hovered && !io.WantTextInput && ImGui::IsKeyPressed('F')) g.fitRequested = true;    // F：全体を表示

                // 左ボタンを押した瞬間
                if (activated) {
                    const int hs = hitState();
                    if (g.linkFrom != kNoLink) {
                        // 遷移を作っている途中：別の状態をクリックしたら、そこへの遷移を作る
                        if (hs >= 0 && hs != g.linkFrom) {
                            SM::Transition tr; tr.from = g.linkFrom; tr.to = hs;
                            sm->transitions.push_back(std::move(tr));
                            g.selKind = SelKind::Transition; g.selIndex = (int)sm->transitions.size() - 1;     // 続けて右の欄で条件を付けられるように
                        }
                        g.linkFrom = kNoLink;
                        g.dragTarget = kDragNothing;
                    }
                    else if (hs >= 0) { g.selKind = SelKind::State; g.selIndex = hs; g.dragTarget = hs; }
                    else if (hasAnyBox() && inRect(toScreen(sm->anyX, sm->anyY), anySize)) { g.dragTarget = kDragAny; }
                    else {
                        const int ht = hitTransition();
                        if (ht >= 0) { g.selKind = SelKind::Transition; g.selIndex = ht; g.dragTarget = kDragNothing; }
                        else { g.ClearSelection(); g.dragTarget = kDragPan; }
                    }
                }
                if (active) {
                    // 箱は、マウスを少し動かしてから動かし始める（クリックやダブルクリックで、箱がずれないように）。
                    // 動かす量は、画面上の移動量を拡大率で割って「グラフ上の長さ」に直す
                    const bool dragging = ImGui::IsMouseDragging(0, 4.0f);
                    if (g.dragTarget >= 0 && g.dragTarget < (int)sm->states.size()) {
                        if (dragging) { sm->states[g.dragTarget].editorX += io.MouseDelta.x / z; sm->states[g.dragTarget].editorY += io.MouseDelta.y / z; }
                    }
                    else if (g.dragTarget == kDragAny) { if (dragging) { sm->anyX += io.MouseDelta.x / z; sm->anyY += io.MouseDelta.y / z; } }
                    else if (g.dragTarget == kDragPan) { g.scroll.x += io.MouseDelta.x; g.scroll.y += io.MouseDelta.y; }
                }
                else g.dragTarget = kDragNothing;

                // 状態をダブルクリック：その状態の中へ入る
                if (hovered && ImGui::IsMouseDoubleClicked(0) && g.linkFrom == kNoLink) {
                    const int hs = hitState();
                    if (hs >= 0) openState = hs;
                }

                // Delete キー：選んでいるものを消す（文字を打っている最中は効かせない）
                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput && ImGui::IsKeyPressed(0x2E)) {
                    if (g.selKind == SelKind::State) { sm->RemoveState(g.selIndex); g.ClearSelection(); openState = -1; }
                    else if (g.selKind == SelKind::Transition) { sm->transitions.erase(sm->transitions.begin() + g.selIndex); g.ClearSelection(); }
                }

                // 右クリックのメニュー
                if (ImGui::BeginPopup("ag_ctx")) {
                    if (g.ctxKind == SelKind::State && g.ctxIndex >= 0 && g.ctxIndex < (int)sm->states.size()) {
                        ImGui::TextDisabled("%s", sm->states[g.ctxIndex].name.c_str());
                        ImGui::Separator();
                        if (ImGui::MenuItem("Open")) openState = g.ctxIndex;
                        if (ImGui::MenuItem("Add Transition from here")) g.linkFrom = g.ctxIndex;
                        if (ImGui::MenuItem("Set as Entry", nullptr, false, sm->entry != g.ctxIndex)) sm->entry = g.ctxIndex;
                        if (ImGui::MenuItem("Delete State")) { sm->RemoveState(g.ctxIndex); g.ClearSelection(); openState = -1; }
                    }
                    else if (g.ctxKind == SelKind::Transition && g.ctxIndex >= 0 && g.ctxIndex < (int)sm->transitions.size()) {
                        if (ImGui::MenuItem("Delete Transition")) { sm->transitions.erase(sm->transitions.begin() + g.ctxIndex); g.ClearSelection(); }
                    }
                    else {
                        if (ImGui::MenuItem("Add State")) {
                            SM::State st;
                            st.name = "State" + std::to_string(sm->states.size());
                            st.node = MakeClipNode(model);
                            st.editorX = g.ctxGraphPos.x - kNodeW * 0.5f; st.editorY = g.ctxGraphPos.y - kNodeH * 0.5f;    // 右クリックした場所に置く
                            if (st.editorX == 0.0f && st.editorY == 0.0f) st.editorX = 1.0f;                                // (0,0) は「まだ置いていない」の印なので避ける
                            sm->states.push_back(std::move(st));
                            g.selKind = SelKind::State; g.selIndex = (int)sm->states.size() - 1;
                        }
                        if (ImGui::MenuItem("Add Transition from Any", nullptr, false, !sm->states.empty())) g.linkFrom = -1;
                        if (ImGui::MenuItem("Fit to view  (F)")) g.fitRequested = true;
                    }
                    ImGui::EndPopup();
                }

                // マウスを重ねた状態の説明（何も押していないときだけ）
                if (hovered && !active && !g.panning && g.linkFrom == kNoLink && !ImGui::IsPopupOpen("ag_ctx")) {
                    const int hs = hitState();
                    if (hs >= 0) {
                        const SM::State& st = sm->states[hs];
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(st.name.c_str());
                        ImGui::TextDisabled("%s", NodeSummary(st.node.get()).c_str());
                        if (auto* cn = dynamic_cast<AnimClipNode*>(st.node.get())) {
                            const AnimationClip* clip = model ? cn->FindClip(*model) : nullptr;
                            if (clip) ImGui::TextDisabled("time %.2f / %.2f s%s", cn->time, clip->duration, cn->loop ? "  (loop)" : "");
                            else if (model) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "clip not found in this model");
                        }
                        if (hs == sm->current) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "playing now");
                        if (hs == sm->entry) ImGui::TextDisabled("entry state");
                        ImGui::EndTooltip();
                    }
                }

                // ---- 描画 ----
                const int stateCount = (int)sm->states.size();      // 上の入力で増減しているかもしれないので、ここで数え直す
                if (g.linkFrom >= stateCount) g.linkFrom = kNoLink;  // 「ここから」の状態が消されていたら、作りかけの遷移をやめる
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);      // キャンバスの外にはみ出した分は描かない
                DrawCanvasBackground(dl, origin, size, g.scroll, z);

                const ImU32 colSelected = IM_COL32(255, 255, 255, 255);
                const ImU32 colLive = IM_COL32(255, 210, 90, 255);
                const float thin = (z < 0.7f) ? 1.0f : 1.5f;

                // 遷移（矢印）。箱の下に隠れるように、先に描く
                const bool fading = sm->IsFading();
                for (int i = 0; i < (int)sm->transitions.size(); ++i) {
                    const SM::Transition& tr = sm->transitions[i];
                    ImVec2 p0, p1;
                    if (!transitionEnds(i, p0, p1)) continue;
                    // いま起きている遷移（移る前 → 今）は、色を変えて太くする
                    const bool live = fading && !sm->fromSnapshot && tr.to == sm->current && (tr.from == sm->fromState || tr.from == -1);
                    const bool isSel = (g.selKind == SelKind::Transition && g.selIndex == i);
                    const ImU32 col = isSel ? colSelected : (live ? colLive : IM_COL32(150, 156, 170, 255));
                    DrawArrow(dl, p0, p1, col, (live || isSel) ? 3.0f : thin, TransitionLabel(tr, an->params).c_str(), z);
                }

                // 作っている途中の遷移：「ここから」の箱からマウスまで、線を伸ばす
                if (g.linkFrom != kNoLink) {
                    const ImVec2 from = (g.linkFrom == -1) ? anyCenter() : stateCenter(g.linkFrom);
                    dl->AddLine(from, io.MousePos, colLive, 2.0f);
                    dl->AddCircleFilled(io.MousePos, 4.0f, colLive);
                }

                // Any の箱
                if (hasAnyBox()) {
                    const ImVec2 mn = toScreen(sm->anyX, sm->anyY);
                    const ImVec2 mx(mn.x + anySize.x, mn.y + anySize.y);
                    const ImVec2 c = anyCenter();
                    dl->AddRectFilled(mn, mx, IM_COL32(70, 60, 90, 255), 14.0f * z);
                    dl->AddRect(mn, mx, (g.linkFrom == -1) ? colLive : IM_COL32(150, 130, 190, 255), 14.0f * z);
                    const ImVec2 ts = TextSizeZ("Any", z);
                    AddTextZ(dl, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32(230, 230, 240, 255), "Any", z);
                }

                // 状態（箱）
                for (int i = 0; i < stateCount; ++i) {
                    const SM::State& st = sm->states[i];
                    const ImVec2 mn = toScreen(st.editorX, st.editorY);
                    const ImVec2 mx(mn.x + nodeSize.x, mn.y + nodeSize.y);
                    const bool isNow = (i == sm->current);
                    const bool isFrom = fading && !sm->fromSnapshot && i == sm->fromState && !isNow;
                    const bool isSel = (g.selKind == SelKind::State && g.selIndex == i);
                    const bool isLinkSrc = (g.linkFrom == i);

                    dl->AddRectFilled(mn, mx, isNow ? IM_COL32(40, 78, 52, 255) : IM_COL32(52, 56, 66, 255), 6.0f * z);
                    const ImU32 border = isNow ? IM_COL32(110, 230, 130, 255) : ((isFrom || isLinkSrc) ? colLive : IM_COL32(110, 116, 130, 255));
                    dl->AddRect(mn, mx, border, 6.0f * z, ImDrawCornerFlags_All, (isNow || isFrom || isLinkSrc) ? 2.5f : 1.0f);
                    if (isSel)      // 選んでいる箱は、外側に白い枠をもう1本
                        dl->AddRect(ImVec2(mn.x - 3, mn.y - 3), ImVec2(mx.x + 3, mx.y + 3), colSelected, 8.0f * z, ImDrawCornerFlags_All, 1.5f);

                    dl->PushClipRect(mn, ImVec2(mx.x - 4 * z, mx.y), true);        // 長い名前は箱の中で切る
                    AddTextZ(dl, ImVec2(mn.x + 8 * z, mn.y + 6 * z), IM_COL32(235, 238, 245, 255), st.name.c_str(), z);
                    AddTextZ(dl, ImVec2(mn.x + 8 * z, mn.y + 25 * z), IM_COL32(150, 156, 170, 255), NodeSummary(st.node.get()).c_str(), z);
                    dl->PopClipRect();

                    // ブレンドの進み具合を、今の状態の箱の下のふちに出す
                    if (isNow && fading) {
                        float a = sm->fadeElapsed / sm->fadeDuration; if (a > 1.0f) a = 1.0f;
                        dl->AddRectFilled(ImVec2(mn.x + 4 * z, mx.y - 5 * z), ImVec2(mn.x + 4 * z + (nodeSize.x - 8 * z) * a, mx.y - 2 * z), colLive, 2.0f);
                    }

                    // 最初の状態には、左から「Entry」の矢印を付ける
                    if (i == sm->entry) {
                        const float cy = mn.y + halfNode.y;
                        const ImU32 ec = IM_COL32(110, 200, 255, 255);
                        const ImVec2 ts = TextSizeZ("Entry", z);
                        AddTextZ(dl, ImVec2(mn.x - 34 * z - ts.x, cy - ts.y * 0.5f), ec, "Entry", z);
                        dl->AddLine(ImVec2(mn.x - 30 * z, cy), ImVec2(mn.x - 8 * z, cy), ec, 2.0f);
                        dl->AddTriangleFilled(ImVec2(mn.x - 1, cy), ImVec2(mn.x - 10 * z, cy - 5 * z), ImVec2(mn.x - 10 * z, cy + 5 * z), ec);
                    }
                }

                if (stateCount == 0) dl->AddText(ImVec2(origin.x + 16, origin.y + 16), IM_COL32(150, 156, 170, 255), "Right-click: Add State");
                dl->PopClipRect();
            }
            ImGui::EndChild();

            // ---------------- 右：選んでいるものの編集欄 ----------------
            ImGui::SameLine();
            ImGui::BeginChild("ag_details", ImVec2(0.0f, 0.0f), true);
            {
                const ImVec4 warn(1.0f, 0.8f, 0.3f, 1.0f);

                if (g.selKind == SelKind::State && g.selIndex >= 0 && g.selIndex < (int)sm->states.size()) {
                    SM::State& st = sm->states[g.selIndex];
                    ImGui::TextUnformatted("State");
                    ImGui::Separator();

                    char nb[64]; strncpy_s(nb, sizeof(nb), st.name.c_str(), _TRUNCATE);
                    if (ImGui::InputText("Name", nb, sizeof(nb))) st.name = nb;

                    if (sm->entry == g.selIndex) ImGui::TextDisabled("This is the entry state.");
                    else if (ImGui::SmallButton("Set as Entry")) sm->entry = g.selIndex;

                    // 中身の種類（変えると作り直す）
                    const char* nodeTypes[] = { "Clip", "BlendSpace1D" };
                    int ty = (st.node && std::strcmp(st.node->TypeName(), "BlendSpace1D") == 0) ? 1 : 0;
                    if (ImGui::Combo("Content", &ty, nodeTypes, 2) || !st.node) {
                        if (ty == 0) st.node = MakeClipNode(model);
                        else         st.node = AnimNodePtr(std::make_unique<AnimBlendSpace1DNode>());
                    }

                    ImGui::Separator();
                    if (model) DrawAnimNodeDetails(st.node.get(), *model, an->params);      // クリップや Blend Space の中身
                    else ImGui::TextColored(warn, "No model: set a Skinned GLB to pick clips.");

                    ImGui::Separator();
                    if (ImGui::SmallButton("Open")) openState = g.selIndex;
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Add Transition from here")) g.linkFrom = g.selIndex;
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Delete State")) { sm->RemoveState(g.selIndex); g.ClearSelection(); openState = -1; }
                }
                else if (g.selKind == SelKind::Transition && g.selIndex >= 0 && g.selIndex < (int)sm->transitions.size()) {
                    SM::Transition& tr = sm->transitions[g.selIndex];
                    ImGui::TextUnformatted("Transition");
                    ImGui::Separator();

                    auto stateName = [&](int i) -> const char* {
                        if (i < 0) return "(Any)";
                        return (i < (int)sm->states.size()) ? sm->states[i].name.c_str() : "?";
                        };
                    auto stateCombo = [&](const char* label, int& idx, bool allowAny) {
                        if (ImGui::BeginCombo(label, stateName(idx))) {
                            if (allowAny && ImGui::Selectable("(Any)", idx == -1)) idx = -1;
                            for (int i = 0; i < (int)sm->states.size(); ++i) {
                                ImGui::PushID(i);
                                if (ImGui::Selectable(sm->states[i].name.c_str(), idx == i)) idx = i;
                                ImGui::PopID();
                            }
                            ImGui::EndCombo();
                        }
                        };
                    stateCombo("From", tr.from, true);
                    stateCombo("To", tr.to, false);
                    ImGui::DragFloat("Blend Time", &tr.blendTime, 0.01f, 0.0f, 2.0f, "%.2f s");
                    ImGui::Checkbox("At End", &tr.waitForEnd);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Wait until the clip of the 'From' state has finished.\nWorks only when that clip has Loop off.");

                    // 条件（すべて成り立ったら移る）
                    ImGui::Separator();
                    ImGui::TextUnformatted("Conditions (all must be true)");
                    const char* floatOps[] = { ">", "<" };
                    const char* boolOps[] = { "is true", "is false" };
                    int removeCond = -1;
                    for (int k = 0; k < (int)tr.conditions.size(); ++k) {
                        SM::Condition& c = tr.conditions[k];
                        ImGui::PushID(k);
                        ImGui::SetNextItemWidth(110);
                        if (ImGui::BeginCombo("##p", c.param.empty() ? "(none)" : c.param.c_str())) {
                            for (int i = 0; i < (int)an->params.items.size(); ++i) {
                                ImGui::PushID(i);
                                if (ImGui::Selectable(an->params.items[i].name.c_str(), an->params.items[i].name == c.param)) c.param = an->params.items[i].name;
                                ImGui::PopID();
                            }
                            ImGui::EndCombo();
                        }
                        const int pi = an->params.Find(c.param);
                        if (pi >= 0 && an->params.items[pi].type == AnimParamType::Float) {
                            int op = (c.op == SM::CondOp::Less) ? 1 : 0;
                            ImGui::SameLine(); ImGui::SetNextItemWidth(45);
                            if (ImGui::Combo("##op", &op, floatOps, 2)) c.op = op ? SM::CondOp::Less : SM::CondOp::Greater;
                            ImGui::SameLine(); ImGui::SetNextItemWidth(70);
                            ImGui::DragFloat("##th", &c.threshold, 0.01f);
                        }
                        else if (pi >= 0 && an->params.items[pi].type == AnimParamType::Bool) {
                            int op = (c.op == SM::CondOp::IsFalse) ? 1 : 0;
                            ImGui::SameLine(); ImGui::SetNextItemWidth(85);
                            if (ImGui::Combo("##op", &op, boolOps, 2)) c.op = op ? SM::CondOp::IsFalse : SM::CondOp::IsTrue;
                        }
                        else if (pi >= 0) { ImGui::SameLine(); ImGui::TextDisabled("is fired"); }
                        else { ImGui::SameLine(); ImGui::TextColored(warn, "(pick a parameter)"); }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("x")) removeCond = k;
                        ImGui::PopID();
                    }
                    if (removeCond >= 0) tr.conditions.erase(tr.conditions.begin() + removeCond);
                    if (ImGui::SmallButton("+ Condition")) tr.conditions.push_back(SM::Condition{});
                    if (an->params.items.empty()) ImGui::TextColored(warn, "No parameters yet. Add them on the left.");
                    if (tr.conditions.empty() && !tr.waitForEnd) ImGui::TextColored(warn, "Never fires: add a condition or At End.");

                    ImGui::Separator();
                    if (ImGui::SmallButton("Delete Transition")) { sm->transitions.erase(sm->transitions.begin() + g.selIndex); g.ClearSelection(); }
                }
                else {
                    ImGui::TextUnformatted("Nothing selected");
                    ImGui::Separator();
                    ImGui::TextDisabled("Click a state or an arrow to edit it.");
                    ImGui::TextDisabled("Double-click a state to open it.");
                    ImGui::TextDisabled("Right-click the background: Add State.");
                    ImGui::TextDisabled("Right-click a state: Add Transition.");
                    ImGui::TextDisabled("Mouse wheel: zoom.  F: fit to view.");
                }
            }
            ImGui::EndChild();
            return openState;
        }

    } // namespace

    void DrawAnimGraphPanel(World& world, EntityId selected, bool playMode, ID3D11Device* device)
    {
        ImGui::Begin("Anim Graph");

        Entity* e = world.Get(selected);
        AnimatorComponent* an = e ? e->GetComponent<AnimatorComponent>() : nullptr;
        if (!an) { ImGui::TextUnformatted("Select an Entity that has an AnimatorComponent."); ImGui::End(); return; }

        // クリップの名前を選ぶために、同じ Entity のモデルを取っておく（無ければ nullptr）
        std::shared_ptr<SkinnedRenderModel> modelHolder;
        if (SkinnedMeshComponent* sk = e->GetComponent<SkinnedMeshComponent>())
            if (!sk->assetPath.empty()) modelHolder = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
        const SkinnedModel* model = (modelHolder && modelHolder->Valid()) ? &modelHolder->Cpu() : nullptr;

        // 別の Entity を選んだら、選択も、見ている階層も、最初に戻す
        if (g.lastEntity != selected) { g.lastEntity = selected; g.ResetForNewGraph(); }

        // ---- 上の1行目：グラフのアセット ----
        if (DrawAssetRow(an)) {
            // 読み込むとノードが作り直される（今まで見ていたノードは無くなる）ので、このフレームはここで終える
            g.ResetForNewGraph();
            ImGui::End();
            return;
        }

        // ---- 見ている階層が、今のグラフでまだ成り立つかを確かめる ----
        SM* rootSM = dynamic_cast<SM*>(an->graph.get());
        if (!rootSM) g.level = Level::AnimGraph;                                    // 根がステートマシンでなければ、中には入れない
        if (g.level == Level::State && (g.levelState < 0 || g.levelState >= (int)rootSM->states.size())) g.level = Level::StateMachine;

        // ---- 上の2行目：いまの場所（押すと、その階層へ戻る）と、表示の操作 ----
        {
            if (ImGui::SmallButton("AnimGraph")) g.level = Level::AnimGraph;
            if (rootSM && g.level != Level::AnimGraph) {
                ImGui::SameLine(); ImGui::TextDisabled(">"); ImGui::SameLine();
                if (ImGui::SmallButton("State Machine")) g.level = Level::StateMachine;
            }
            if (rootSM && g.level == Level::State) {
                ImGui::SameLine(); ImGui::TextDisabled(">"); ImGui::SameLine();
                ImGui::Text("State: %s", rootSM->states[g.levelState].name.c_str());
            }

            ImGui::SameLine(); ImGui::TextDisabled("  |");
            ImGui::SameLine();
            if (ImGui::SmallButton("Fit")) g.fitRequested = true;
            ImGui::SameLine();
            const float curZoom = (g.level == Level::StateMachine) ? g.zoom : g.poseZoom;
            ImGui::TextDisabled("%d%%", (int)(curZoom * 100.0f + 0.5f));

            ImGui::SameLine();
            if (g.level == Level::StateMachine) {
                const bool nowValid = (rootSM->current >= 0 && rootSM->current < (int)rootSM->states.size());
                ImGui::Text("   Now: %s", nowValid ? rootSM->states[rootSM->current].name.c_str() : "-");
                if (g.linkFrom != kNoLink) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.35f, 1.0f), "   Click the target state  (Esc / right-click: cancel)"); }
            }
            else ImGui::TextUnformatted("");
            if (playMode) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "  PLAY (live, edits are not kept)"); }
        }

        // ---- 左：パラメータ ----
        DrawParamsPane(an);
        ImGui::SameLine();

        // ---- 中央と右：見ている階層 ----
        if (g.level == Level::StateMachine) {
            const int open = DrawStateMachineView(an, rootSM, model);
            if (open >= 0 && open < (int)rootSM->states.size()) { g.level = Level::State; g.levelState = open; g.poseSelected = true; }
        }
        else if (g.level == Level::State) {
            DrawPoseView(an, rootSM->states[g.levelState].node, false, model);     // 状態の中には、ステートマシンを置かない
        }
        else {
            if (DrawPoseView(an, an->graph, true, model)) { g.level = Level::StateMachine; g.ClearSelection(); g.fitRequested = true; }
        }

        ImGui::End();
    }

} // namespace uel
