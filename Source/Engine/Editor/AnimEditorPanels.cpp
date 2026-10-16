#include "Engine/Editor/EditorPanels.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/SkinnedMeshComponent.h"
#include "Engine/Scene/AnimatorComponent.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Animation/AnimBlendSpaceNode.h"
#include "Engine/Animation/AnimStateMachineNode.h"

#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// アニメ関連の編集 UI（グラフのノードの編集欄、通知の編集欄、Animation パネル）。
namespace uel {

    namespace {
        // グラフのノード1つぶんの編集欄（ノードの種類ごと）。
        // ノードエディタができるまでの仮の置き場所で、今は根のノードだけを編集する。
        void DrawAnimNodeInspector(AnimNode* node, const SkinnedModel& model, AnimParams& params)
        {
            if (!node) { ImGui::TextDisabled("(no node)"); return; }

            // クリップを名前で選ぶ
            auto clipCombo = [&](const char* label, std::string& name) {
                if (ImGui::BeginCombo(label, name.empty() ? "(none)" : name.c_str())) {
                    for (int i = 0; i < (int)model.clips.size(); ++i) {
                        ImGui::PushID(i);
                        if (ImGui::Selectable(model.clips[i].name.c_str(), model.clips[i].name == name)) name = model.clips[i].name;
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                };
            // パラメータを名前で選ぶ（floatOnly = true なら Float だけを並べる）
            auto paramCombo = [&](const char* label, std::string& name, bool floatOnly) {
                if (ImGui::BeginCombo(label, name.empty() ? "(none)" : name.c_str())) {
                    if (ImGui::Selectable("(none)", name.empty())) name.clear();
                    for (int i = 0; i < (int)params.items.size(); ++i) {
                        if (floatOnly && params.items[i].type != AnimParamType::Float) continue;
                        ImGui::PushID(i);
                        if (ImGui::Selectable(params.items[i].name.c_str(), params.items[i].name == name)) name = params.items[i].name;
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                };

            if (auto* cn = dynamic_cast<AnimClipNode*>(node)) {
                clipCombo("Clip", cn->clipName);
                ImGui::Checkbox("Loop", &cn->loop);
                paramCombo("Speed Param", cn->speedParam, true);
                const AnimationClip* nclip = cn->FindClip(model);
                const float ndur = (nclip && nclip->duration > 0.0f) ? nclip->duration : 0.0f;
                ImGui::TextDisabled("time %.2f / %.2f s%s", cn->time, ndur, cn->finished ? "  (finished)" : "");

                // ---- 通知：再生位置がこの時刻を通り過ぎたら、名前をゲーム側へ知らせる ----
                ImGui::Separator();
                ImGui::TextUnformatted("Notifies");


                DrawNotifyEditor(cn->notifies, ndur, cn->time);     // タイムライン＋一覧（モンタージュの編集欄と同じもの）
            }
            else if (auto* bs = dynamic_cast<AnimBlendSpace1DNode*>(node)) {
                paramCombo("Axis Param", bs->param, true);
                ImGui::DragFloat("Smooth Time", &bs->smoothTime, 0.01f, 0.0f, 2.0f, "%.2f s");

                // サンプル（クリップと、軸の上の位置）
                int removeAt = -1;
                for (int i = 0; i < (int)bs->samples.size(); ++i) {
                    auto& s = bs->samples[i];
                    ImGui::PushID(i);
                    ImGui::SetNextItemWidth(170);
                    clipCombo("##clip", s.clipName);
                    ImGui::SameLine(); ImGui::SetNextItemWidth(70);
                    ImGui::DragFloat("##pos", &s.position, 0.01f, 0.0f, 0.0f, "%.2f");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x")) removeAt = i;
                    if (i == bs->lower || i == bs->upper) {         // いま混ざっているサンプルに、割合を出す
                        const float w = (bs->lower == bs->upper) ? 1.0f : ((i == bs->upper) ? bs->weight : 1.0f - bs->weight);
                        ImGui::SameLine(); ImGui::Text("%3.0f%%", w * 100.0f);
                    }
                    ImGui::PopID();
                }
                if (removeAt >= 0) bs->samples.erase(bs->samples.begin() + removeAt);
                if (ImGui::SmallButton("+ Sample")) {
                    AnimBlendSpace1DNode::Sample s;
                    if (!model.clips.empty()) s.clipName = model.clips[0].name;
                    for (const auto& o : bs->samples) if (o.position >= s.position) s.position = o.position + 1.0f;
                    bs->samples.push_back(s);
                }

                // 軸の表示：サンプルの位置（縦線）と、いまの値（赤い印）
                if (!bs->samples.empty()) {
                    float mn = bs->samples[0].position, mx = mn;
                    for (const auto& s : bs->samples) { if (s.position < mn) mn = s.position; if (s.position > mx) mx = s.position; }
                    if (mx - mn < 1e-4f) mx = mn + 1.0f;
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 p0 = ImGui::GetCursorScreenPos();
                    float w = ImGui::GetContentRegionAvail().x; if (w < 40.0f) w = 40.0f;
                    const float h = 22.0f, pad = 8.0f;
                    ImGui::Dummy(ImVec2(w, h));                       // 場所だけ確保する（線の描画は部品ではない）
                    auto toX = [&](float v) { return p0.x + pad + (v - mn) / (mx - mn) * (w - pad * 2.0f); };
                    dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + h), IM_COL32(30, 32, 38, 255), 4.0f);
                    for (const auto& s : bs->samples)
                        dl->AddLine(ImVec2(toX(s.position), p0.y + 3), ImVec2(toX(s.position), p0.y + h - 3), IM_COL32(120, 180, 255, 255), 2.0f);
                    float v = bs->value; if (v < mn) v = mn; if (v > mx) v = mx;
                    dl->AddTriangleFilled(ImVec2(toX(v) - 5, p0.y), ImVec2(toX(v) + 5, p0.y), ImVec2(toX(v), p0.y + 9), IM_COL32(255, 90, 90, 255));
                }
                ImGui::TextDisabled("value %.2f   phase %.2f", bs->value, bs->phase);
            }
            else if (auto* sm = dynamic_cast<AnimStateMachineNode*>(node)) {
                using SM = AnimStateMachineNode;
                const ImVec4 warn(1.0f, 0.8f, 0.3f, 1.0f);

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
                auto makeClipNode = [&]() {
                    auto n = std::make_unique<AnimClipNode>();
                    if (!model.clips.empty()) n->clipName = model.clips[0].name;
                    return AnimNodePtr(std::move(n));
                    };

                // いまの状態
                if (sm->current >= 0 && sm->current < (int)sm->states.size()) {
                    ImGui::Text("Now: %s", sm->states[sm->current].name.c_str());
                    if (sm->IsFading()) { ImGui::SameLine(); ImGui::Text("(blending %.0f%%)", sm->fadeElapsed / sm->fadeDuration * 100.0f); }
                }
                else ImGui::TextDisabled("Now: -");

                // ---- 状態 ----
                ImGui::TextUnformatted("States  (o = entry)");
                const char* nodeTypes[] = { "Clip", "BlendSpace1D" };
                int removeState = -1;
                for (int i = 0; i < (int)sm->states.size(); ++i) {
                    SM::State& st = sm->states[i];
                    ImGui::PushID(i);
                    if (ImGui::RadioButton("##entry", sm->entry == i)) sm->entry = i;      // 最初の状態を選ぶ
                    ImGui::SameLine();
                    char nb[64]; strncpy_s(nb, sizeof(nb), st.name.c_str(), _TRUNCATE);
                    ImGui::SetNextItemWidth(110);
                    if (ImGui::InputText("##name", nb, sizeof(nb))) st.name = nb;
                    ImGui::SameLine();
                    int ty = (st.node && std::strcmp(st.node->TypeName(), "BlendSpace1D") == 0) ? 1 : 0;
                    ImGui::SetNextItemWidth(110);
                    if (ImGui::Combo("##type", &ty, nodeTypes, 2) || !st.node) {
                        if (ty == 0) st.node = makeClipNode();
                        else         st.node = AnimNodePtr(std::make_unique<AnimBlendSpace1DNode>());
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x")) removeState = i;
                    if (i == sm->current) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "<- now"); }
                    if (ImGui::TreeNode("Edit")) {
                        DrawAnimNodeInspector(st.node.get(), model, params);    // 状態の中身（クリップや Blend Space）の編集欄
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (removeState >= 0) sm->RemoveState(removeState);
                if (ImGui::SmallButton("+ State")) {
                    SM::State st;
                    st.name = "State" + std::to_string(sm->states.size());
                    st.node = makeClipNode();
                    sm->states.push_back(std::move(st));
                }

                // ---- 遷移 ----
                ImGui::Separator();
                ImGui::TextUnformatted("Transitions  (checked from top)");
                const char* floatOps[] = { ">", "<" };
                const char* boolOps[] = { "is true", "is false" };
                int removeTr = -1;
                for (int i = 0; i < (int)sm->transitions.size(); ++i) {
                    SM::Transition& tr = sm->transitions[i];
                    ImGui::PushID(1000 + i);
                    ImGui::SetNextItemWidth(95); stateCombo("##from", tr.from, true);
                    ImGui::SameLine(); ImGui::TextUnformatted("->");
                    ImGui::SameLine(); ImGui::SetNextItemWidth(95); stateCombo("##to", tr.to, false);
                    ImGui::SameLine(); ImGui::SetNextItemWidth(60);
                    ImGui::DragFloat("##blend", &tr.blendTime, 0.01f, 0.0f, 2.0f, "%.2fs");
                    ImGui::SameLine(); ImGui::Checkbox("At End", &tr.waitForEnd);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x")) removeTr = i;

                    // 条件（すべて成り立ったら移る）
                    int removeCond = -1;
                    for (int k = 0; k < (int)tr.conditions.size(); ++k) {
                        SM::Condition& c = tr.conditions[k];
                        ImGui::PushID(k);
                        ImGui::TextUnformatted("    if"); ImGui::SameLine();
                        ImGui::SetNextItemWidth(100); paramCombo("##p", c.param, false);
                        const int pi = params.Find(c.param);
                        if (pi >= 0 && params.items[pi].type == AnimParamType::Float) {
                            int op = (c.op == SM::CondOp::Less) ? 1 : 0;
                            ImGui::SameLine(); ImGui::SetNextItemWidth(45);
                            if (ImGui::Combo("##op", &op, floatOps, 2)) c.op = op ? SM::CondOp::Less : SM::CondOp::Greater;
                            ImGui::SameLine(); ImGui::SetNextItemWidth(70);
                            ImGui::DragFloat("##th", &c.threshold, 0.01f);
                        }
                        else if (pi >= 0 && params.items[pi].type == AnimParamType::Bool) {
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
                    ImGui::TextUnformatted("   "); ImGui::SameLine();
                    if (ImGui::SmallButton("+ Condition")) tr.conditions.push_back(SM::Condition{});
                    if (tr.conditions.empty() && !tr.waitForEnd) { ImGui::SameLine(); ImGui::TextColored(warn, "never fires (no condition)"); }
                    ImGui::PopID();
                }
                if (removeTr >= 0) sm->transitions.erase(sm->transitions.begin() + removeTr);
                if (ImGui::SmallButton("+ Transition")) {
                    SM::Transition tr;
                    tr.from = sm->states.empty() ? -1 : 0;
                    tr.to = (sm->states.size() > 1) ? 1 : 0;
                    sm->transitions.push_back(std::move(tr));
                }
            }
        }
    }

    void DrawAnimNodeDetails(AnimNode* node, const SkinnedModel& model, AnimParams& params)
    {
        DrawAnimNodeInspector(node, model, params);
    }

    // 通知の編集欄（タイムライン＋一覧）。クリップのノードと、モンタージュの両方から使う。
    //   duration … クリップの長さ（秒）。0 以下ならタイムラインを出さない（一覧だけ）
    //   playTime … いまの再生位置（秒）。赤い線で出す。マイナスなら出さない
    void DrawNotifyEditor(std::vector<AnimClipNode::Notify>& notifies, float duration, float playTime)
    {
        // タイムライン：横がクリップの長さ。赤い線が今の再生位置、黄色い印が通知。印はドラッグで動かせる
        if (duration > 0.0f) {
            static const void* dragOwner = nullptr;     // 印をつかんでいる一覧（つかんでいなければ nullptr）
            static int         dragIndex = -1;
            const void* self = &notifies;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x; if (w < 60.0f) w = 60.0f;
            const float h = 34.0f;
            ImGui::InvisibleButton("notify_bar", ImVec2(w, h));     // マウスを受け取る部品。線や印は、この上に描くだけ
            const bool barActive = ImGui::IsItemActive();
            const bool barActivated = ImGui::IsItemActivated();
            const bool barHovered = ImGui::IsItemHovered();
            const float mouseX = ImGui::GetIO().MousePos.x;
            auto toX = [&](float t) { return p0.x + (t / duration) * w; };
            auto toT = [&](float x) {
                float t = (x - p0.x) / w * duration;
                if (t < 0.0f) t = 0.0f;
                if (t > duration) t = duration;
                return t;
                };

            // マウスにいちばん近い印（6 ピクセル以内）
            int hoverIndex = -1; float best = 6.0f;
            for (int i = 0; i < (int)notifies.size(); ++i) {
                float d = toX(notifies[i].time) - mouseX; if (d < 0.0f) d = -d;
                if (d <= best) { best = d; hoverIndex = i; }
            }
            if (barActivated) {
                dragOwner = self; dragIndex = hoverIndex;
                if (hoverIndex < 0 && ImGui::IsMouseDoubleClicked(0)) {     // 何もない所をダブルクリック：そこに通知を足す
                    AnimClipNode::Notify n; n.time = toT(mouseX); n.name = "Hit";
                    notifies.push_back(n);
                    dragIndex = (int)notifies.size() - 1;                   // そのままドラッグで位置を直せる
                }
            }
            if (barActive && dragOwner == self && dragIndex >= 0 && dragIndex < (int)notifies.size())
                notifies[dragIndex].time = toT(mouseX);
            if (!barActive && dragOwner == self) { dragOwner = nullptr; dragIndex = -1; }

            // 描画
            const ImVec2 p1(p0.x + w, p0.y + h);
            dl->AddRectFilled(p0, p1, IM_COL32(30, 32, 38, 255), 4.0f);
            dl->AddRect(p0, p1, IM_COL32(80, 84, 92, 255), 4.0f);
            if (playTime >= 0.0f) {
                const float pt = (playTime > duration) ? duration : playTime;
                dl->AddLine(ImVec2(toX(pt), p0.y), ImVec2(toX(pt), p1.y), IM_COL32(255, 90, 90, 255), 2.0f);     // 今の再生位置
            }
            dl->PushClipRect(p0, p1, true);
            for (int i = 0; i < (int)notifies.size(); ++i) {
                const float x = toX(notifies[i].time);
                const bool hot = (i == hoverIndex && barHovered) || (dragOwner == self && i == dragIndex);
                const ImU32 col = hot ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 210, 90, 255);
                dl->AddLine(ImVec2(x, p0.y + 14), ImVec2(x, p1.y), col, 2.0f);
                dl->AddTriangleFilled(ImVec2(x - 5, p0.y + 14), ImVec2(x + 5, p0.y + 14), ImVec2(x, p0.y + 21), col);
                dl->AddText(ImVec2(x + 4, p0.y + 1), col, notifies[i].name.c_str());
            }
            dl->PopClipRect();
            if (barHovered && !barActive && hoverIndex >= 0)
                ImGui::SetTooltip("%s   %.2f s", notifies[hoverIndex].name.c_str(), notifies[hoverIndex].time);
            ImGui::TextDisabled("drag a marker / double-click to add");
        }

        // 一覧：時刻と名前を数字と文字で直す、消す
        int removeN = -1;
        for (int i = 0; i < (int)notifies.size(); ++i) {
            AnimClipNode::Notify& n = notifies[i];
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(80);
            ImGui::DragFloat("##t", &n.time, 0.005f, 0.0f, (duration > 0.0f) ? duration : 100.0f, "%.2f s");
            ImGui::SameLine();
            char nb[48]; strncpy_s(nb, sizeof(nb), n.name.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputText("##n", nb, sizeof(nb))) n.name = nb;
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) removeN = i;
            ImGui::PopID();
        }
        if (removeN >= 0) notifies.erase(notifies.begin() + removeN);
        if (ImGui::SmallButton("+ Notify at current time")) {
            AnimClipNode::Notify n; n.time = (playTime > 0.0f) ? playTime : 0.0f; n.name = "Hit";
            notifies.push_back(n);
        }
    }

    void DrawAnimationEditorPanel(World& world, EntityId selected, ID3D11Device* device)
    {
        ImGui::Begin("Animation");
        Entity* e = world.Get(selected);
        SkinnedMeshComponent* sk = e ? e->GetComponent<SkinnedMeshComponent>() : nullptr;
        if (!sk) { ImGui::TextUnformatted("Select an Entity that has a SkinnedMeshComponent."); ImGui::End(); return; }
        if (sk->assetPath.empty()) { ImGui::TextUnformatted("Skinned GLB is not set."); ImGui::End(); return; }

        auto model = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
        if (!model || !model->Valid()) { ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "load failed"); ImGui::End(); return; }
        const auto& clips = model->Cpu().clips;
        if (clips.empty()) { ImGui::TextUnformatted("No clips. (If Anim GLB is set, its bone names do not match this model.)"); ImGui::End(); return; }

        // 再生の状態は、AnimatorComponent があればそちら、無ければ SkinnedMeshComponent のものを使う
        AnimatorComponent* an = e->GetComponent<AnimatorComponent>();
        if (an && an->graph) {
            ImGui::TextUnformatted("This Entity is driven by an animation graph. Timeline preview works in clip mode only.");
            ImGui::End(); return;
        }
        int& clipIndex = an ? an->clipIndex : sk->clipIndex;
        float& time = an ? an->time : sk->time;
        float& speed = an ? an->speed : sk->speed;
        bool& playing = an ? an->playing : sk->playing;
        bool& loop = an ? an->loop : sk->loop;
        if (clipIndex < 0 || clipIndex >= (int)clips.size()) clipIndex = 0;

        // 左：クリップ一覧
        ImGui::BeginChild("clips", ImVec2(170, 0), true);
        ImGui::TextUnformatted("Clips"); ImGui::Separator();
        for (int i = 0; i < (int)clips.size(); ++i) {
            std::string nm = clips[i].name.empty() ? ("clip " + std::to_string(i)) : clips[i].name;
            char buf[96]; snprintf(buf, sizeof(buf), "%s  (%.2fs)", nm.c_str(), clips[i].duration);
            if (ImGui::Selectable(buf, i == clipIndex)) {
                if (an) an->Play(i);                    // クロスフェードで切り替える
                else { clipIndex = i; time = 0.0f; }    // すぐ切り替える
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("timeline", ImVec2(0, 0), false);

        const AnimationClip& clip = clips[clipIndex];
        float dur = clip.duration > 0.0f ? clip.duration : 1.0f;

        // トランスポート
        if (ImGui::Button(playing ? "Pause" : "Play")) playing = !playing;
        ImGui::SameLine();
        if (ImGui::Button("Stop")) { playing = false; time = 0.0f; if (an) an->CancelFade(); }
        ImGui::SameLine(); ImGui::Checkbox("Loop", &loop);
        ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::DragFloat("Speed", &speed, 0.01f, -4.0f, 4.0f);
        ImGui::Text("Time  %.3f / %.3f s    Bones %d",
            time, dur, (int)model->Cpu().skeleton.bones.size());
        if (an) {
            ImGui::SameLine();
            if (an->IsFading()) ImGui::Text("   Animator: blending %.0f%%", an->fadeElapsed / an->fadeDuration * 100.0f);
            else                ImGui::TextDisabled("   Animator: click a clip to crossfade");
        }

        // タイムライン（キーフレーム＋プレイヘッド、ドラッグでスクラブ）
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x; if (w < 20.0f) w = 20.0f;
        float h = 64.0f;
        ImVec2 p1 = ImVec2(p0.x + w, p0.y + h);
        ImGui::InvisibleButton("tl", ImVec2(w, h));
        bool active = ImGui::IsItemActive();

        dl->AddRectFilled(p0, p1, IM_COL32(30, 32, 38, 255), 4.0f);
        dl->AddRect(p0, p1, IM_COL32(80, 84, 92, 255), 4.0f);

        // 目盛り（0.5秒毎）
        for (float t = 0.0f; t <= dur + 1e-3f; t += 0.5f) {
            float x = p0.x + (t / dur) * w;
            dl->AddLine(ImVec2(x, p1.y - 10), ImVec2(x, p1.y), IM_COL32(90, 94, 104, 255));
            char lbl[16]; snprintf(lbl, sizeof(lbl), "%.1f", t);
            dl->AddText(ImVec2(x + 2, p0.y + 2), IM_COL32(150, 154, 164, 255), lbl);
        }
        // キーフレーム（回転トラックのキー時刻）
        for (const auto& tr : clip.tracks)
            for (const auto& k : tr.rot) {
                float x = p0.x + (k.time / dur) * w;
                dl->AddCircleFilled(ImVec2(x, p0.y + h * 0.55f), 1.8f, IM_COL32(120, 180, 255, 110));
            }
        // スクラブ
        if (active) {
            float t = (ImGui::GetIO().MousePos.x - p0.x) / w * dur;
            if (t < 0) t = 0; if (t > dur) t = dur;
            time = t; playing = false;
            if (an) an->CancelFade();       // 止めて確認するときは、混ぜずにそのクリップだけを見せる
        }
        // プレイヘッド
        float px = p0.x + (time / dur) * w;
        dl->AddLine(ImVec2(px, p0.y), ImVec2(px, p1.y), IM_COL32(255, 90, 90, 255), 2.0f);
        dl->AddTriangleFilled(ImVec2(px - 5, p0.y), ImVec2(px + 5, p0.y), ImVec2(px, p0.y + 8), IM_COL32(255, 90, 90, 255));

        ImGui::EndChild();
        ImGui::End();
    }

} // namespace uel
