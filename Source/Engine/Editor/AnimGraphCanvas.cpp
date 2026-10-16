#include "Engine/Editor/AnimGraphPanelInternal.h"
#include "Engine/Editor/EditorPanels.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Animation/AnimBlendSpaceNode.h"

#include <imgui.h>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

// Anim Graph パネルの描画の補助（拡大した文字・矢印・背景・ラベル・ノードの生成）と、ポーズグラフの表示。
// 共有の状態 g の実体もここに置く。
namespace uel {
    namespace animgraph {
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

    } // namespace animgraph
} // namespace uel
