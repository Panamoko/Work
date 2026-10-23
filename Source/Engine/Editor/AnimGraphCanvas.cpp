#include "Engine/Editor/AnimGraphPanelInternal.h"
#include "Engine/Editor/EditorPanels.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Animation/AnimBlendSpaceNode.h"
#include "Engine/Animation/AnimLayeredBlendNode.h"

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
            if (tr.waitForEnd) s += s.empty() ? u8"終了時" : u8" & 終了時";
            if (s.empty()) s = u8"（条件なし）";
            return s;
        }

        // ノードの中身を1行で（例： "Clip: walk"、"BlendSpace1D (3)"、"StateMachine (3 states)"）
        std::string NodeSummary(AnimNode* n)
        {
            if (!n) return u8"（空）";
            if (auto* cn = dynamic_cast<AnimClipNode*>(n)) return "Clip: " + cn->clipName;
            if (auto* bs = dynamic_cast<AnimBlendSpace1DNode*>(n)) return "BlendSpace1D" + std::string(u8"（") + std::to_string(bs->samples.size()) + u8"）";
            if (auto* sm = dynamic_cast<SM*>(n)) return "StateMachine" + std::string(u8"（") + std::to_string(sm->states.size()) + u8" 状態）";
            if (auto* lb = dynamic_cast<AnimLayeredBlendNode*>(n))
                return std::string(lb->basePose ? lb->basePose->TypeName() : "-") + " + " + (lb->blendPose ? lb->blendPose->TypeName() : "-")
                + u8"（" + (lb->maskBone.empty() ? std::string(u8"全身") : lb->maskBone) + u8"）";
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
        // 骨ごとのレイヤー。いまのノード（old）があれば、それを「下地」に入れる（例：移動のステートマシンを下半身に残す）
        AnimNodePtr MakeLayeredBlendNode(const SkinnedModel* model, AnimNodePtr old)
        {
            auto n = std::make_unique<AnimLayeredBlendNode>();
            n->basePose = old ? std::move(old) : MakeClipNode(model);
            n->blendPose = MakeClipNode(model);
            if (model) {    // 骨の名前に "spine" を含む最初の骨を、重ねる範囲の初期値にする（無ければ全身）
                for (const SkeletonBone& b : model->skeleton.bones) {
                    std::string lower = b.name;
                    for (char& c : lower) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
                    if (lower.find("spine") != std::string::npos) { n->maskBone = b.name; break; }
                }
            }
            return AnimNodePtr(std::move(n));
        }

        // =====================================================================
        //  ポーズグラフの階層（AnimGraph / State）： ノード → Output Pose
        //    slot             … 編集するノードの入れ物（AnimGraph なら根、State ならその状態の中身）
        //    allowStateMachine … ステートマシンや LayeredBlend を置けるか（根だけ）
        //  ノードが LayeredBlend なら、左に2つの入力（下地・重ねる側）の箱も描く。
        //  戻り値：ダブルクリックなどで開くステートマシン（0 = このノード、1 = 下地、2 = 重ねる側）。無ければ -1
        // =====================================================================
        int DrawPoseView(AnimatorComponent* an, AnimNodePtr& slot, bool allowStateMachine, const SkinnedModel* model)
        {
            ImGuiIO& io = ImGui::GetIO();
            int enter = -1;

            AnimLayeredBlendNode* layered = dynamic_cast<AnimLayeredBlendNode*>(slot.get());
            AnimNodePtr* inputs[2] = { layered ? &layered->basePose : nullptr, layered ? &layered->blendPose : nullptr };
            const char* inputNames[2] = { u8"下地", u8"重ねる側" };
            if (!layered) g.poseInput = -1;                 // 入力の箱は LayeredBlend のときだけ

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

                // 箱の位置（グラフ上の位置は固定。動かせるのは画面だけ）
                //   ふつう        ：ノード (0,0) → 出力ポーズ
                //   LayeredBlend ：下地 (0,0) と重ねる側 (0,下) → ノード（右）→ 出力ポーズ
                const float inGapY = kPoseNodeH + 40.0f;                        // 2つの入力の箱の、縦の間隔
                const float nodeX = layered ? kPoseNodeW + 90.0f : 0.0f;
                const float nodeY = layered ? inGapY * 0.5f : 0.0f;

                // 「全体を表示」：箱が全部入る大きさ（最大 100%）にして、真ん中に置く。ホイール：拡大縮小
                if (g.fitRequested) {
                    g.fitRequested = false;
                    const float gw = nodeX + kOutputX + kOutputW, gh = nodeY * 2.0f + kPoseNodeH;     // 箱全体の幅と高さ（グラフ上）
                    const float zx = (size.x - 60.0f) / gw, zy = (size.y - 60.0f) / gh;
                    float nz = (zx < zy) ? zx : zy;
                    if (nz > 1.0f) nz = 1.0f;
                    if (nz < kZoomMin) nz = kZoomMin;
                    g.poseZoom = nz;
                    g.poseScroll = ImVec2((size.x - gw * nz) * 0.5f, (size.y - gh * nz) * 0.5f);
                }
                if (hovered) ApplyWheelZoom(origin, g.poseScroll, g.poseZoom);
                const float z = g.poseZoom;
                auto toScreen = [&](float x, float y) { return ImVec2(origin.x + g.poseScroll.x + x * z, origin.y + g.poseScroll.y + y * z); };
                const ImVec2 nodeMin = toScreen(nodeX, nodeY);
                const ImVec2 nodeMax(nodeMin.x + kPoseNodeW * z, nodeMin.y + kPoseNodeH * z);
                const ImVec2 outMin = toScreen(nodeX + kOutputX, nodeY);
                const ImVec2 outMax(outMin.x + kOutputW * z, outMin.y + kPoseNodeH * z);
                ImVec2 inMin[2], inMax[2];
                for (int k = 0; k < 2; ++k) {
                    inMin[k] = toScreen(0.0f, inGapY * k);
                    inMax[k] = ImVec2(inMin[k].x + kPoseNodeW * z, inMin[k].y + kPoseNodeH * z);
                }
                auto mouseIn = [&](ImVec2 a, ImVec2 b) { return io.MousePos.x >= a.x && io.MousePos.x <= b.x && io.MousePos.y >= a.y && io.MousePos.y <= b.y; };
                const bool hasNode = (bool)slot;
                const bool overNode = hasNode && mouseIn(nodeMin, nodeMax);
                int overInput = -1;                                             // マウスが乗っている入力の箱（0 = 下地、1 = 重ねる側）
                if (layered) for (int k = 0; k < 2; ++k) if (mouseIn(inMin[k], inMax[k])) overInput = k;

                // 入力：右ドラッグで画面の移動、動かさずに離せばメニュー
                if (hovered && ImGui::IsMouseClicked(1)) { g.posePanning = true; g.poseRightDownPos = io.MousePos; }
                if (hovered && ImGui::IsMouseClicked(2)) g.posePanning = true;
                if (g.posePanning) {
                    if (io.MouseDown[1] || io.MouseDown[2]) { g.poseScroll.x += io.MouseDelta.x; g.poseScroll.y += io.MouseDelta.y; }
                    else g.posePanning = false;
                }
                if (hovered && ImGui::IsMouseReleased(1)) {
                    const float mdx = io.MousePos.x - g.poseRightDownPos.x, mdy = io.MousePos.y - g.poseRightDownPos.y;
                    if (mdx * mdx + mdy * mdy < 16.0f) {
                        if (overInput >= 0) { g.poseSelected = true; g.poseInput = overInput; ImGui::OpenPopup("ag_pose_in_ctx"); }   // 入力の箱：その入力を置く／置き換える
                        else { if (overNode) { g.poseSelected = true; g.poseInput = -1; } ImGui::OpenPopup("ag_pose_ctx"); }
                    }
                }
                if (activated) {
                    g.poseSelected = overNode || overInput >= 0;
                    g.poseInput = overInput;
                }
                if (active && !g.poseSelected) { g.poseScroll.x += io.MouseDelta.x; g.poseScroll.y += io.MouseDelta.y; }     // 背景をつかんだ：画面を動かす
                if (hovered && ImGui::IsMouseDoubleClicked(0)) {
                    if (overNode && dynamic_cast<SM*>(slot.get())) enter = 0;
                    if (overInput >= 0 && dynamic_cast<SM*>(inputs[overInput]->get())) enter = 1 + overInput;
                }
                if (hovered && !io.WantTextInput && ImGui::IsKeyPressed('F')) g.fitRequested = true;

                // 右クリックのメニュー：ノードを置く／置き換える／消す
                if (ImGui::BeginPopup("ag_pose_ctx")) {
                    ImGui::TextDisabled(hasNode ? u8"ノードを置き換える" : u8"ノードを追加");
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clip")) { slot = MakeClipNode(model); g.poseSelected = true; g.poseInput = -1; }
                    if (ImGui::MenuItem("Blend Space 1D")) { slot = AnimNodePtr(std::make_unique<AnimBlendSpace1DNode>()); g.poseSelected = true; g.poseInput = -1; }
                    if (allowStateMachine && ImGui::MenuItem("State Machine")) { slot = MakeStateMachineNode(model); g.poseSelected = true; g.poseInput = -1; }
                    if (allowStateMachine && ImGui::MenuItem(u8"Layered Blend（上半身に重ねる）")) {
                        const bool keep = slot && !dynamic_cast<AnimLayeredBlendNode*>(slot.get());     // いまのノードは「下地」に残す
                        slot = MakeLayeredBlendNode(model, keep ? std::move(slot) : AnimNodePtr());
                        g.poseSelected = true; g.poseInput = -1;
                        g.fitRequested = true;      // 箱が増えるので、全体が見えるように合わせる
                    }
                    if (hasNode) {
                        ImGui::Separator();
                        if (ImGui::MenuItem(u8"ノードを削除")) { slot.reset(); g.poseSelected = false; g.poseInput = -1; }
                    }
                    ImGui::EndPopup();
                }
                // 入力の箱の右クリックのメニュー（LayeredBlend の下地・重ねる側）
                if (ImGui::BeginPopup("ag_pose_in_ctx")) {
                    if (layered && g.poseInput >= 0 && g.poseInput < 2) {
                        AnimNodePtr& in = *inputs[g.poseInput];
                        ImGui::TextDisabled(u8"%s：%s", inputNames[g.poseInput], in ? u8"ノードを置き換える" : u8"ノードを追加");
                        ImGui::Separator();
                        if (ImGui::MenuItem("Clip")) in = MakeClipNode(model);
                        if (ImGui::MenuItem("Blend Space 1D")) in = AnimNodePtr(std::make_unique<AnimBlendSpace1DNode>());
                        if (ImGui::MenuItem("State Machine")) in = MakeStateMachineNode(model);
                        if (in) {
                            ImGui::Separator();
                            if (ImGui::MenuItem(u8"ノードを削除")) in.reset();
                        }
                    }
                    ImGui::EndPopup();
                }

                // 描画
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
                DrawCanvasBackground(dl, origin, size, g.poseScroll, z);

                const ImU32 colWire = IM_COL32(120, 200, 255, 255);
                const ImU32 colPinOff = IM_COL32(90, 94, 104, 255);
                const ImVec2 outPin(outMin.x, outMin.y + kPoseNodeH * z * 0.5f);        // Output Pose の入力ピン（左の辺の中央）

                // ノードの箱を1つ描く（selected：白い枠、title：1行目、sub：2行目）
                auto drawBox = [&](ImVec2 mn, ImVec2 mx, ImU32 fill, bool selected, const char* title, const std::string& sub) {
                    dl->AddRectFilled(mn, mx, fill, 6.0f * z);
                    dl->AddRect(mn, mx, IM_COL32(110, 116, 130, 255), 6.0f * z);
                    if (selected)
                        dl->AddRect(ImVec2(mn.x - 3, mn.y - 3), ImVec2(mx.x + 3, mx.y + 3), IM_COL32(255, 255, 255, 255), 8.0f * z, ImDrawCornerFlags_All, 1.5f);
                    dl->PushClipRect(mn, ImVec2(mx.x - 8 * z, mx.y), true);
                    AddTextZ(dl, ImVec2(mn.x + 10 * z, mn.y + 8 * z), IM_COL32(235, 238, 245, 255), title, z);
                    AddTextZ(dl, ImVec2(mn.x + 10 * z, mn.y + 32 * z), IM_COL32(150, 156, 170, 255), sub.c_str(), z);
                    dl->PopClipRect();
                    };
                auto nodeFill = [&](AnimNode* n) {
                    if (dynamic_cast<SM*>(n)) return IM_COL32(46, 58, 82, 255);
                    if (dynamic_cast<AnimLayeredBlendNode*>(n)) return IM_COL32(58, 50, 78, 255);
                    return IM_COL32(52, 56, 66, 255);
                    };
                auto wire = [&](ImVec2 a, ImVec2 b) {      // 横に出てから曲がる線
                    dl->AddBezierCurve(a, ImVec2(a.x + 60 * z, a.y), ImVec2(b.x - 60 * z, b.y), b, colWire, 2.5f * z);
                    };

                // Output Pose の箱
                dl->AddRectFilled(outMin, outMax, IM_COL32(60, 50, 44, 255), 6.0f * z);
                dl->AddRect(outMin, outMax, IM_COL32(210, 160, 110, 255), 6.0f * z);
                AddTextZ(dl, ImVec2(outMin.x + 16 * z, outMin.y + 8 * z), IM_COL32(235, 238, 245, 255), u8"出力ポーズ", z);
                AddTextZ(dl, ImVec2(outMin.x + 16 * z, outMin.y + 30 * z), IM_COL32(150, 156, 170, 255), u8"結果", z);
                dl->AddCircleFilled(outPin, 5.0f * z, slot ? colWire : colPinOff);

                if (slot) {
                    AnimNode* n = slot.get();
                    const ImVec2 nodePin(nodeMax.x, nodeMin.y + kPoseNodeH * z * 0.5f); // ノードの出力ピン（右の辺の中央）
                    wire(nodePin, outPin);
                    const bool isSM = dynamic_cast<SM*>(n) != nullptr;
                    drawBox(nodeMin, nodeMax, nodeFill(n), g.poseSelected && g.poseInput < 0, n->TypeName(),
                        isSM ? std::string(u8"ダブルクリックで開く") : NodeSummary(n));
                    dl->AddCircleFilled(nodePin, 5.0f * z, colWire);

                    // LayeredBlend：左の2つの入力の箱と、ノードの入力ピン（上 = 下地、下 = 重ねる側）
                    if (layered) {
                        for (int k = 0; k < 2; ++k) {
                            const ImVec2 inPin(nodeMin.x, nodeMin.y + kPoseNodeH * z * (k == 0 ? 0.3f : 0.7f));
                            AnimNode* c = inputs[k]->get();
                            const std::string title = std::string(inputNames[k]) + u8"：" + (c ? c->TypeName() : u8"（空）");
                            std::string sub;
                            if (!c) sub = u8"右クリックで追加";
                            else if (dynamic_cast<SM*>(c)) sub = u8"ダブルクリックで開く";
                            else sub = NodeSummary(c);
                            if (k == 1) sub += u8"（" + (layered->maskBone.empty() ? std::string(u8"全身") : layered->maskBone) + u8"）";
                            drawBox(inMin[k], inMax[k], c ? nodeFill(c) : IM_COL32(36, 38, 44, 255), g.poseSelected && g.poseInput == k, title.c_str(), sub);
                            const ImVec2 outPinK(inMax[k].x, inMin[k].y + kPoseNodeH * z * 0.5f);
                            if (c) { wire(outPinK, inPin); dl->AddCircleFilled(outPinK, 5.0f * z, colWire); }
                            dl->AddCircleFilled(inPin, 5.0f * z, c ? colWire : colPinOff);
                        }
                    }
                }
                else {
                    AddTextZ(dl, ImVec2(nodeMin.x, nodeMin.y + 20 * z), IM_COL32(150, 156, 170, 255), u8"右クリック：ノードを追加", z);
                }
                dl->PopClipRect();
            }
            ImGui::EndChild();

            // ---------------- 右：ノードの編集欄 ----------------
            ImGui::SameLine();
            ImGui::BeginChild("ag_pose_details", ImVec2(0.0f, 0.0f), true);
            {
                // 選んでいるもの：ノード本体か、LayeredBlend の入力のどちらか
                AnimNode* n = slot.get();
                if (layered && g.poseInput >= 0 && g.poseInput < 2) n = inputs[g.poseInput]->get();
                if (n && g.poseSelected) {
                    if (layered && g.poseInput >= 0) ImGui::Text(u8"%s：%s ノード", inputNames[g.poseInput], n->TypeName());
                    else ImGui::Text(u8"%s ノード", n->TypeName());
                    ImGui::Separator();
                    if (auto* sm = dynamic_cast<SM*>(n)) {
                        ImGui::Text(u8"状態 %d　遷移 %d", (int)sm->states.size(), (int)sm->transitions.size());
                        if (ImGui::Button(u8"State Machine を開く")) enter = (layered && g.poseInput >= 0) ? 1 + g.poseInput : 0;
                    }
                    else if (model) DrawAnimNodeDetails(n, *model, an->params);     // クリップ・Blend Space・LayeredBlend の中身
                    else ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), u8"モデルがありません。\nGLB を設定するとクリップを選べます。");
                }
                else {
                    ImGui::TextUnformatted(n ? u8"何も選んでいません" : u8"ノードがありません");
                    ImGui::Separator();
                    ImGui::TextDisabled(u8"左のノードが出力ポーズになります");
                    ImGui::TextDisabled(u8"ノードをクリックで編集");
                    ImGui::TextDisabled(u8"右クリック：追加／置き換え／削除");
                    if (allowStateMachine) ImGui::TextDisabled(u8"ダブルクリックで中を開く");
                }
            }
            ImGui::EndChild();
            return enter;
        }

    } // namespace animgraph
} // namespace uel
