#include "Engine/Editor/AnimGraphPanelInternal.h"
#include "Engine/Editor/EditorPanels.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Animation/AnimBlendSpaceNode.h"

#include <imgui.h>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

// Anim Graph パネルのステートマシンの表示（状態の箱と遷移の矢印。選択・移動・追加・削除と、右の編集欄）。
namespace uel {
    namespace animgraph {

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

    } // namespace animgraph
} // namespace uel
