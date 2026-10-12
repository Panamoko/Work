#include "Engine/Editor/EditorPanels.h"
#include "Engine/Editor/TransformEdit.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Serialization/StageSerializer.h"   
#include "Engine/Editor/EntityCommands.h"   
#include "Engine/Config/EngineConfig.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Scene/HPComponent.h"
#include "Engine/Game/PlayerComponent.h"
#include "Engine/Game/OpponentComponent.h"
#include "Engine/Graphics/SceneLighting.h"
#include "Engine/Scene/SkinnedMeshComponent.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/AI/EngineNavGrid.h"
#include "Engine/AI/NavObstacleComponent.h"
#include "Engine/Game/ScreenUIComponent.h"
#include "Engine/Game/GameRuleComponent.h"
#include "Engine/Scene/PrefabInstanceComponent.h"
#include "Engine/Game/HitboxComponent.h"
#include "Engine/Game/SpawnerComponent.h"
#include "Engine/Scene/ShaderComponent.h"
#include "Engine/Graphics/PostProcessSettings.h"
#include "Engine/Scene/AnimatorComponent.h"
#include "Engine/Animation/AnimBlendSpaceNode.h"
#include "Engine/Animation/AnimStateMachineNode.h"
#include "Engine/Serialization/AnimGraphSerializer.h"

#include <memory>
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <map>
namespace uel {

    namespace {
        bool        g_assetRescan = false;   // Prefab保存後に Asset Browser を再スキャンさせる
        std::string g_prefabRefresh;         // Apply されたPrefab（Engineが全Sceneへ反映）

        bool IsPrefabPath(const std::string& p) {
            const std::string suf = ".prefab.json";
            return p.size() >= suf.size() && p.compare(p.size() - suf.size(), suf.size(), suf) == 0;
        }
        bool IsAnimGraphPath(const std::string& p) {
            const std::string suf = ".animgraph.json";
            return p.size() >= suf.size() && p.compare(p.size() - suf.size(), suf.size(), suf) == 0;
        }
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
                ImGui::TextDisabled("time %.2f s%s", cn->time, cn->finished ? "  (finished)" : "");
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

    std::string ConsumePrefabRefreshRequest()
    {
        std::string p = g_prefabRefresh;
        g_prefabRefresh.clear();
        return p;
    }

    void DrawHierarchyPanel(World& world, EntityId& selected, CommandHistory& history)
    {
        ImGui::Begin("Hierarchy");

        if (ImGui::Button("+ Create"))
            selected = EditorCreateEntity(world, history, "Entity");

        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            if (world.Get(selected)) {
                EntityId id = EditorDuplicateEntity(world, history, selected);
                if (id.IsValid()) selected = id;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            if (world.Get(selected)) {
                EditorDeleteEntity(world, history, selected);
                selected = EntityId{};
            }
        }
        ImGui::Separator();

        static EntityId renamingId{};
        static char     renameBuf[128] = "";
        static bool     renameFocus = false;

        // F2：選択中Entityをリネーム（0x71 = VK_F2）
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(0x71, false)) {
            if (Entity* se = world.Get(selected)) {
                renamingId = selected;
                strncpy_s(renameBuf, sizeof(renameBuf), se->name.c_str(), _TRUNCATE);
                renameFocus = true;
            }
        }

        for (const auto& slot : world.Slots())
        {
            if (!slot) continue;
            Entity* e = slot.get();

            if (e->id == renamingId) {                                  // リネーム中の行は入力欄
                if (renameFocus) { ImGui::SetKeyboardFocusHere(); renameFocus = false; }
                ImGui::SetNextItemWidth(-1);
                char rid[32]; snprintf(rid, sizeof(rid), "##rename%u", e->id.index);
                bool enter = ImGui::InputText(rid, renameBuf, sizeof(renameBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                if (enter || ImGui::IsItemDeactivated()) {              // Enter or 他をクリックで確定（Escは元の名前に戻る）
                    EditorRenameEntity(world, history, e->id, e->name, renameBuf);
                    renamingId = EntityId{};
                }
                continue;
            }

            bool isSelected = (e->id == selected);
            bool isPrefab = (e->GetComponent<PrefabInstanceComponent>() != nullptr);
            char label[128];
            snprintf(label, sizeof(label), "%s##%u", e->name.c_str(), e->id.index);
            if (isPrefab) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.70f, 1.0f, 1.0f));
            bool clicked = ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_AllowDoubleClick);
            if (isPrefab) ImGui::PopStyleColor();
            if (clicked) {
                selected = e->id;
                if (ImGui::IsMouseDoubleClicked(0)) {                   // ダブルクリックでリネーム開始
                    renamingId = e->id;
                    strncpy_s(renameBuf, sizeof(renameBuf), e->name.c_str(), _TRUNCATE);
                    renameFocus = true;
                }
            }
        
        }
        ImGui::End();
    }

    void DrawInspectorPanel(World& world, EntityId selected, CommandHistory& history,
        TransformEditSession& session, MeshCache& cache, ID3D11Device* device)
    {
        ImGui::Begin("Inspector");
        Entity* e = world.Get(selected);
        if (!e) { ImGui::TextUnformatted("(No selection)"); ImGui::End(); return; }

        {
            static std::string renameOld;
            std::string before = e->name;
            char nameBuf[128]; strncpy_s(nameBuf, sizeof(nameBuf), e->name.c_str(), _TRUNCATE);
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) e->name = nameBuf;
            if (ImGui::IsItemActivated()) renameOld = before;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                std::string newName = e->name;
                e->name = renameOld;                                   // いったん戻してから
                EditorRenameEntity(world, history, selected, renameOld, newName);   // Undo付きで適用
            }
        }
        ImGui::Text("Id   : %u (gen %u)", e->id.index, e->id.generation);
        {
            static std::string prefabMsg;
            static float       prefabMsgTimer = 0.0f;

            if (auto* pi = e->GetComponent<PrefabInstanceComponent>()) {
                // ---- Prefabのインスタンス ----
                std::string ppath = pi->path;   // Apply/Revertで部品が作り直されるので先にコピー
                ImGui::TextColored(ImVec4(0.45f, 0.70f, 1.0f, 1.0f), "Prefab: %s", ppath.c_str());
                if (ImGui::Button("Apply to Prefab")) {
                    bool ok = StageSerializer::SavePrefab(*e, ppath.c_str());
                    if (ok) g_prefabRefresh = ppath;                   // 全Sceneの同じPrefabへ反映
                    prefabMsg = ok ? "Applied" : "Apply failed";
                    prefabMsgTimer = 3.0f;
                }
                ImGui::SameLine();
                if (ImGui::Button("Revert")) {
                    bool ok = StageSerializer::RevertPrefabInstance(*e);
                    prefabMsg = ok ? "Reverted" : "Revert failed (prefab missing?)";
                    prefabMsgTimer = 3.0f;
                }
                ImGui::SameLine();
                if (ImGui::Button("Unpack")) e->RemoveComponent<PrefabInstanceComponent>();
                ImGui::TextDisabled("Per-instance: Name/Transform only. Apply to keep other changes.");
            }
            else if (ImGui::Button("Save as Prefab")) {
                // ---- 普通のEntity → Prefabとして保存し、このEntityをそのインスタンスにする ----
                std::string file;
                for (char ch : e->name)
                    file += (std::isalnum((unsigned char)ch) || ch == '_' || ch == '-') ? ch : '_';
                if (file.empty()) file = "Prefab";
                std::string path = std::string(kAssetRoot) + "/Prefabs/" + file + ".prefab.json";
                bool ok = StageSerializer::SavePrefab(*e, path.c_str());
                if (ok) {
                    e->AddComponent<PrefabInstanceComponent>()->path = path;
                    g_prefabRefresh = path;                          // 同名Prefabの既存インスタンスも更新
                }
                prefabMsg = ok ? ("Saved: " + path) : std::string("Save failed");
                prefabMsgTimer = 3.0f;
                g_assetRescan = true;
            }

            if (prefabMsgTimer > 0.0f) {
                prefabMsgTimer -= ImGui::GetIO().DeltaTime;
                ImGui::SameLine(); ImGui::TextUnformatted(prefabMsg.c_str());
            }
        }
        if (TransformComponent* t = e->GetComponent<TransformComponent>())
        {
            ImGui::Separator(); ImGui::TextUnformatted("Transform");
            ImGui::DragFloat3("Position", &t->position.x, 0.05f);
            if (ImGui::IsItemActivated())            BeginTransformEdit(session, selected, *t);
            if (ImGui::IsItemDeactivatedAfterEdit()) CommitTransformEdit(session, world, history);
            ImGui::DragFloat3("Rotation", &t->rotationEuler.x, 0.5f);
            if (ImGui::IsItemActivated())            BeginTransformEdit(session, selected, *t);
            if (ImGui::IsItemDeactivatedAfterEdit()) CommitTransformEdit(session, world, history);
            ImGui::DragFloat3("Scale", &t->scale.x, 0.05f, 0.001f, 1000.0f);
            if (ImGui::IsItemActivated())            BeginTransformEdit(session, selected, *t);
            if (ImGui::IsItemDeactivatedAfterEdit()) CommitTransformEdit(session, world, history);
        }

        if (MeshRendererComponent* m = e->GetComponent<MeshRendererComponent>())
        {
            ImGui::PushID("cmp_meshrenderer");
            ImGui::Separator(); ImGui::TextUnformatted("MeshRenderer"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<MeshRendererComponent>();
            else {
                char buf[260]; strncpy_s(buf, sizeof(buf), m->assetId.c_str(), _TRUNCATE);
                if (ImGui::InputText("Asset (GLB path)", buf, sizeof(buf))) m->assetId = buf;
                if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の GLB をここへドロップ
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_GLB"))
                        if (pl->Data) m->assetId = static_cast<const char*>(pl->Data);
                    ImGui::EndDragDropTarget();
                }
                ImGui::Checkbox("Visible", &m->visible);
                ImGui::ColorEdit4("Color", &m->color.x);

                // サブメッシュ：役割割当＋可視（このインスタンス）
                if (!m->assetId.empty()) {
                    std::shared_ptr<Mesh> mesh = cache.GetOrLoad(device, m->assetId);
                    if (mesh && mesh->IsValid()) {
                        const auto& subs = mesh->GetSubmeshes();
                        if (m->hiddenSubmesh.size() < subs.size()) m->hiddenSubmesh.resize(subs.size(), 0);
                        if (m->submeshRole.size() < subs.size()) m->submeshRole.resize(subs.size(), 0);

                        ImGui::Separator();
                        ImGui::TextUnformatted("Submeshes / Roles (this instance)");
                        ImGui::Checkbox("First-person (hide FP-hidden)", &m->firstPerson);

                        if (ImGui::Button("Auto-detect head role")) {
                            for (size_t i = 0; i < subs.size(); ++i) {
                                std::string low = subs[i].name;
                                for (char& ch : low) ch = (char)std::tolower((unsigned char)ch);
                                bool headLike = (low.find("head") != std::string::npos)
                                    || (low.find("face") != std::string::npos)
                                    || (low.find("skull") != std::string::npos);
                                m->submeshRole[i] = headLike ? (uint8_t)1 : (uint8_t)0;
                            }
                        }

                        const char* roleNames[] = { "None", "FP-Hidden" };
                        for (size_t i = 0; i < subs.size(); ++i) {
                            ImGui::PushID((int)i);
                            bool vis = (m->hiddenSubmesh[i] == 0);
                            if (ImGui::Checkbox("##vis", &vis)) m->hiddenSubmesh[i] = vis ? 0 : 1;
                            ImGui::SameLine();
                            int role = m->submeshRole[i];
                            ImGui::SetNextItemWidth(110);
                            if (ImGui::Combo("##role", &role, roleNames, 2)) m->submeshRole[i] = (uint8_t)role;
                            ImGui::SameLine();
                            std::string nm = subs[i].name.empty() ? ("submesh " + std::to_string(i)) : subs[i].name;
                            ImGui::TextUnformatted(nm.c_str());
                            ImGui::PopID();
                        }
                    }
                }
            }
            ImGui::PopID();
        }

        // SkinnedMesh は MeshRenderer とは別のブロック（MeshRenderer を外した Entity でも出るように）
        if (SkinnedMeshComponent* sk = e->GetComponent<SkinnedMeshComponent>())
        {
            ImGui::PushID("cmp_skinnedmesh");
            ImGui::Separator(); ImGui::TextUnformatted("SkinnedMesh (Animation)"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<SkinnedMeshComponent>();
            else {
                char sbuf[260]; strncpy_s(sbuf, sizeof(sbuf), sk->assetPath.c_str(), _TRUNCATE);
                if (ImGui::InputText("Skinned GLB", sbuf, sizeof(sbuf))) sk->assetPath = sbuf;
                if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の GLB をここへドロップ
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_GLB"))
                        if (pl->Data) { sk->assetPath = static_cast<const char*>(pl->Data); sk->clipIndex = 0; sk->time = 0.0f; }
                    ImGui::EndDragDropTarget();
                }

                // ---- Anim GLB（アニメだけのファイル。複数可）：1行に1ファイル。ドロップで足し、x で外す ----
                {
                    std::vector<std::string> animFiles = SplitAnimPaths(sk->animPath);
                    std::shared_ptr<SkinnedRenderModel> am;
                    if (!sk->assetPath.empty()) am = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
                    const std::vector<int>* counts = (am && am->Valid()) ? &am->Cpu().clipsFromAnimFiles : nullptr;

                    ImGui::TextUnformatted("Anim GLB (option)");
                    int removeAnim = -1;
                    for (int i = 0; i < (int)animFiles.size(); ++i) {
                        const std::string& f = animFiles[i];
                        ImGui::PushID(i);
                        if (ImGui::SmallButton("x")) removeAnim = i;
                        ImGui::SameLine();
                        ImGui::TextUnformatted(f.substr(f.find_last_of('/') + 1).c_str());     // ファイル名だけ出す
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.c_str());         // 重ねるとフルパス
                        if (counts && i < (int)counts->size()) {
                            ImGui::SameLine();
                            if ((*counts)[i] > 0) ImGui::TextDisabled("+%d clips", (*counts)[i]);
                            else ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "+0 (bones do not match, or same clip names)");
                        }
                        ImGui::PopID();
                    }
                    bool animChanged = false;
                    if (removeAnim >= 0) { animFiles.erase(animFiles.begin() + removeAnim); animChanged = true; }

                    ImGui::Button("Drop Anim GLB here to add", ImVec2(-1.0f, 0.0f));         // ドロップの受け取り先（押しても何もしない）
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_GLB")) {
                            if (pl->Data) {
                                const std::string add = static_cast<const char*>(pl->Data);
                                bool has = false;
                                for (const auto& f : animFiles) if (f == add) has = true;
                                if (!has) { animFiles.push_back(add); animChanged = true; }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (animChanged) { sk->animPath = JoinAnimPaths(animFiles); sk->clipIndex = 0; sk->time = 0.0f; }
                }

                ImGui::Checkbox("Visible##sk", &sk->visible);
                ImGui::ColorEdit4("Tint##sk", &sk->color.x);

                if (!sk->assetPath.empty()) {
                    auto model = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
                    if (model && model->Valid()) {
                        const auto& clips = model->Cpu().clips;
                        ImGui::Text("Bones: %d  Clips: %d",
                            (int)model->Cpu().skeleton.bones.size(), (int)clips.size());
                        if (e->GetComponent<AnimatorComponent>()) {
                            // 再生の状態は AnimatorComponent が持つので、ここでは編集しない
                            ImGui::TextDisabled("Clip / Time are driven by AnimatorComponent.");
                        }
                        else if (!clips.empty()) {
                            if (sk->clipIndex < 0 || sk->clipIndex >= (int)clips.size()) sk->clipIndex = 0;
                            const std::string& preview = clips[sk->clipIndex].name;
                            if (ImGui::BeginCombo("Clip", preview.c_str())) {
                                for (int i = 0; i < (int)clips.size(); ++i) {
                                    bool seld = (i == sk->clipIndex);
                                    std::string label = clips[i].name.empty() ? ("clip " + std::to_string(i)) : clips[i].name;
                                    if (ImGui::Selectable(label.c_str(), seld)) { sk->clipIndex = i; sk->time = 0.0f; }
                                    if (seld) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            float dur = clips[sk->clipIndex].duration;
                            ImGui::Checkbox("Play", &sk->playing); ImGui::SameLine();
                            ImGui::Checkbox("Loop", &sk->loop);
                            ImGui::DragFloat("Speed", &sk->speed, 0.01f, -4.0f, 4.0f);
                            ImGui::SliderFloat("Time", &sk->time, 0.0f, dur > 0.0f ? dur : 1.0f, "%.3f s");
                        }
                        else {
                            ImGui::TextDisabled("No clips in this model.");
                        }
                    }
                    else {
                        ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "load failed");
                    }
                }
            }
            ImGui::PopID();
        }

        // ---- Game Components（UE風の追加/削除）----
        ImGui::Separator();
        ImGui::TextUnformatted("Components");

        // PlayerComponent
        if (auto* pc = e->GetComponent<PlayerComponent>()) {
            ImGui::PushID("cmp_player");
            ImGui::BulletText("PlayerComponent"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<PlayerComponent>();
            else {
                ImGui::DragFloat("MoveSpeed##pl", &pc->moveSpeed, 0.1f, 0.0f, 50.0f);
                ImGui::DragFloat("EyeHeight", &pc->eyeHeight, 0.05f, 0.0f, 20.0f);
                ImGui::DragFloat("EyeForward", &pc->eyeForward, 0.05f, 0.0f, 10.0f);
            }
            ImGui::PopID();
        }

        // OpponentComponent
        if (auto* ec = e->GetComponent<OpponentComponent>()) {
            ImGui::PushID("cmp_opponent");
            ImGui::BulletText("OpponentComponent"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<OpponentComponent>();
            else {
                ImGui::DragFloat("MoveSpeed##en", &ec->moveSpeed, 0.1f, 0.0f, 50.0f);
                ImGui::DragFloat("StopRange", &ec->stopRange, 0.1f, 0.0f, 20.0f);
            }
            ImGui::PopID();
        }

        // HPComponent（表示＋削除）
        if (auto* hpc = e->GetComponent<HPComponent>()) {
            ImGui::PushID("cmp_hp");
            ImGui::BulletText("HPComponent"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<HPComponent>();
            else {
                ImGui::DragFloat("HP", &hpc->hp, 1.0f, 0.0f, hpc->maxHp);
                ImGui::DragFloat("MaxHP", &hpc->maxHp, 1.0f, 1.0f, 10000.0f);
            }
            ImGui::PopID();
        }

        // AnimatorComponent（クリップの切り替えとクロスフェード）
        if (auto* an = e->GetComponent<AnimatorComponent>()) {
            ImGui::PushID("cmp_animator");
            ImGui::BulletText("AnimatorComponent"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<AnimatorComponent>();
            else {
                SkinnedMeshComponent* ask = e->GetComponent<SkinnedMeshComponent>();
                std::shared_ptr<SkinnedRenderModel> amodel;
                if (ask && !ask->assetPath.empty()) amodel = GlobalSkinnedCache().GetOrLoad(device, ask->assetPath, ask->animPath);
                const bool hasClips = amodel && amodel->Valid() && !amodel->Cpu().clips.empty();

                if (!ask) {
                    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Needs a SkinnedMeshComponent on the same Entity.");
                }
                else if (!hasClips) {
                    ImGui::TextDisabled("No clips to play.");
                }
                else if (an->graph) {
                    // ---- グラフで動かしているとき ----
                    ImGui::Text("Graph root: %s node", an->graph->TypeName());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear Graph")) an->graph.reset();
                    if (an->graph) {
                        ImGui::PushID("graph");     // グラフの中の部品が、下の Parameters の部品と同じ ID にならないように分ける
                        DrawAnimNodeInspector(an->graph.get(), amodel->Cpu(), an->params);
                        ImGui::PopID();
                    }
                }
                else {
                    // ---- クリップ1つで動かしているとき ----
                    const auto& clips = amodel->Cpu().clips;
                    if (an->clipIndex < 0 || an->clipIndex >= (int)clips.size()) an->clipIndex = 0;
                    const std::string& preview = clips[an->clipIndex].name;
                    if (ImGui::BeginCombo("Clip", preview.c_str())) {
                        for (int i = 0; i < (int)clips.size(); ++i) {
                            bool seld = (i == an->clipIndex);
                            std::string label = clips[i].name.empty() ? ("clip " + std::to_string(i)) : clips[i].name;
                            if (ImGui::Selectable(label.c_str(), seld)) an->Play(i);      // Blend Time かけて切り替わる
                            if (seld) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::DragFloat("Blend Time", &an->blendTime, 0.01f, 0.0f, 2.0f, "%.2f s");
                    ImGui::Checkbox("Loop", &an->loop);
                    if (an->IsFading()) ImGui::Text("Blending  %.0f%%", an->fadeElapsed / an->fadeDuration * 100.0f);
                    else                ImGui::TextDisabled("Blending  -");

                    // 確認用：グラフを手早く作るボタン（グラフのアセットを読めるようになったら消す）
                    ImGui::TextDisabled("New graph:");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clip")) {
                        auto node = std::make_unique<AnimClipNode>();
                        node->clipName = clips[an->clipIndex].name;
                        node->loop = an->loop;
                        node->speedParam = "PlayRate";
                        if (an->params.Find("PlayRate") < 0) an->params.SetFloat("PlayRate", 1.0f);
                        an->graph = AnimNodePtr(std::move(node));
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("State Machine")) {
                        // 今のクリップを再生する状態を1つだけ持つステートマシン。作った後に状態と遷移を足す
                        auto sm = std::make_unique<AnimStateMachineNode>();
                        AnimStateMachineNode::State st;
                        st.name = "Idle";
                        auto cn = std::make_unique<AnimClipNode>();
                        cn->clipName = clips[an->clipIndex].name;
                        st.node = AnimNodePtr(std::move(cn));
                        sm->states.push_back(std::move(st));
                        an->graph = AnimNodePtr(std::move(sm));
                    }
                }
                // ---- グラフのアセット（Data/Anim/<名前>.animgraph.json）----
                {
                    static std::string graphMsg;
                    static float       graphMsgTimer = 0.0f;
                    char gb[200]; strncpy_s(gb, sizeof(gb), an->graphPath.c_str(), _TRUNCATE);
                    if (ImGui::InputText("Graph Asset", gb, sizeof(gb))) an->graphPath = gb;     // 名前だけでもよい（例：Enemy）
                    if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の [Graph] をここへドロップすると読み込む
                        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_ANIMGRAPH")) {
                            if (pl->Data) {
                                const std::string path = static_cast<const char*>(pl->Data);
                                const bool ok = LoadAnimGraph(path, an->params, an->graph);
                                if (ok) an->graphPath = path;
                                graphMsg = ok ? "Loaded" : "Load failed (file?)";
                                graphMsgTimer = 3.0f;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (ImGui::SmallButton("Save Graph")) {
                        const std::string path = AnimGraphPathFromName(an->graphPath);
                        const bool ok = an->graph && !path.empty() && SaveAnimGraph(path, an->params, an->graph.get());
                        if (ok) { an->graphPath = path; g_assetRescan = true; }
                        graphMsg = ok ? "Saved" : (an->graph ? "Save failed (name?)" : "No graph to save");
                        graphMsgTimer = 3.0f;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Load Graph")) {
                        const std::string path = AnimGraphPathFromName(an->graphPath);
                        const bool ok = !path.empty() && LoadAnimGraph(path, an->params, an->graph);
                        if (ok) an->graphPath = path;
                        graphMsg = ok ? "Loaded" : "Load failed (file?)";
                        graphMsgTimer = 3.0f;
                    }
                    if (graphMsgTimer > 0.0f) {
                        graphMsgTimer -= ImGui::GetIO().DeltaTime;
                        ImGui::SameLine(); ImGui::TextUnformatted(graphMsg.c_str());
                    }
                    ImGui::TextDisabled("Edits to the graph are kept only after Save Graph.");
                }
                ImGui::DragFloat("Play Rate (all)", &an->speed, 0.01f, -4.0f, 4.0f);
                ImGui::Checkbox("Play", &an->playing);

                // ---- パラメータ（グラフに渡す値）----
                ImGui::Separator();
                ImGui::TextUnformatted("Parameters");
                const char* typeNames[] = { "Float", "Bool", "Trigger" };   // same order as AnimParamType
                int removeAt = -1;
                for (int i = 0; i < (int)an->params.items.size(); ++i) {
                    AnimParam& p = an->params.items[i];
                    ImGui::PushID(i);
                    int ty = (int)p.type;
                    ImGui::SetNextItemWidth(70);
                    if (ImGui::Combo("##type", &ty, typeNames, 3)) { p.type = (AnimParamType)ty; p.value = 0.0f; }
                    ImGui::SameLine();
                    char nb[64]; strncpy_s(nb, sizeof(nb), p.name.c_str(), _TRUNCATE);
                    ImGui::SetNextItemWidth(110);
                    if (ImGui::InputText("##name", nb, sizeof(nb))) p.name = nb;
                    ImGui::SameLine();
                    if (p.type == AnimParamType::Float) {
                        ImGui::SetNextItemWidth(90);
                        ImGui::DragFloat("##val", &p.value, 0.01f);
                    }
                    else if (p.type == AnimParamType::Bool) {
                        bool b = p.value > 0.5f;
                        if (ImGui::Checkbox("##val", &b)) p.value = b ? 1.0f : 0.0f;
                    }
                    else {
                        if (ImGui::SmallButton("Fire")) p.value = 1.0f;
                        ImGui::SameLine(); ImGui::TextDisabled(p.value > 0.5f ? "(set)" : "(-)");
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x")) removeAt = i;
                    ImGui::PopID();
                }
                if (removeAt >= 0) an->params.items.erase(an->params.items.begin() + removeAt);
                if (ImGui::SmallButton("+ Parameter")) {
                    AnimParam p; p.name = "Param" + std::to_string(an->params.items.size());
                    an->params.items.push_back(p);
                }
            }
            ImGui::PopID();
        }

        // + Add Component（UEのドロップダウン）
        if (ImGui::Button("+ Add Component")) ImGui::OpenPopup("AddComponentPopup");
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (!e->GetComponent<PlayerComponent>() && ImGui::MenuItem("PlayerComponent")) e->AddComponent<PlayerComponent>();
            if (!e->GetComponent<OpponentComponent>() && ImGui::MenuItem("OpponentComponent"))  e->AddComponent<OpponentComponent>();
            if (!e->GetComponent<HPComponent>() && ImGui::MenuItem("HPComponent"))      e->AddComponent<HPComponent>();
            if (!e->GetComponent<SkinnedMeshComponent>() && ImGui::MenuItem("SkinnedMeshComponent")) e->AddComponent<SkinnedMeshComponent>();
            if (!e->GetComponent<MeshRendererComponent>() && ImGui::MenuItem("MeshRendererComponent")) e->AddComponent<MeshRendererComponent>();
            if (!e->GetComponent<NavObstacleComponent>() && ImGui::MenuItem("NavObstacleComponent")) e->AddComponent<NavObstacleComponent>();
            if (!e->GetComponent<ScreenUIComponent>() && ImGui::MenuItem("ScreenUIComponent")) e->AddComponent<ScreenUIComponent>();
            if (!e->GetComponent<GameRuleComponent>() && ImGui::MenuItem("GameRuleComponent")) e->AddComponent<GameRuleComponent>();
            if (!e->GetComponent<HitboxComponent>() && ImGui::MenuItem("HitboxComponent")) e->AddComponent<HitboxComponent>();
            if (!e->GetComponent<SpawnerComponent>() && ImGui::MenuItem("SpawnerComponent")) e->AddComponent<SpawnerComponent>();
            if (!e->GetComponent<ShaderComponent>() && ImGui::MenuItem("ShaderComponent")) e->AddComponent<ShaderComponent>();
            if (!e->GetComponent<AnimatorComponent>() && ImGui::MenuItem("AnimatorComponent")) e->AddComponent<AnimatorComponent>();

            ImGui::EndPopup();
        }

        if (auto* ob = e->GetComponent<NavObstacleComponent>()) {
            ImGui::PushID("cmp_nav");
            ImGui::BulletText("NavObstacle"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<NavObstacleComponent>();
            else { ImGui::DragFloat("HalfX", &ob->halfX, 0.05f, 0.05f, 50.0f); ImGui::DragFloat("HalfZ", &ob->halfZ, 0.05f, 0.05f, 50.0f); }
            ImGui::PopID();
        }

        auto editStr = [](const char* label, std::string& s) {
            char b[256]; strncpy_s(b, sizeof(b), s.c_str(), _TRUNCATE);
            if (ImGui::InputText(label, b, sizeof(b))) s = b;
            };

        if (auto* ui = e->GetComponent<ScreenUIComponent>()) {
            ImGui::PushID("cmp_ui");
            ImGui::BulletText("ScreenUI (Title/Result)"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<ScreenUIComponent>();
            else {
                editStr("Title", ui->title);
                editStr("Subtitle", ui->subtitle);
                editStr("Button", ui->buttonText);
                editStr("Next Scene", ui->nextScene);
                ImGui::Checkbox("Show Result", &ui->showResult);
            }
            ImGui::PopID();
        }

        if (auto* gr = e->GetComponent<GameRuleComponent>()) {
            ImGui::PushID("cmp_rule");
            ImGui::BulletText("GameRule (win/lose)"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<GameRuleComponent>();
            else {
                editStr("Win Scene", gr->winScene);
                editStr("Lose Scene", gr->loseScene);
                ImGui::DragFloat("End Delay", &gr->endDelay, 0.05f, 0.0f, 10.0f);
            }
            ImGui::PopID();
        }

        if (auto* hb = e->GetComponent<HitboxComponent>()) {
            ImGui::PushID("cmp_hitbox");
            ImGui::BulletText("Hitbox (body capsule + head)"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<HitboxComponent>();
            else {
                ImGui::DragFloat("Body Bottom", &hb->bodyBottom, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Body Top", &hb->bodyTop, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Body Radius", &hb->bodyRadius, 0.01f, 0.01f, 5.0f);
                ImGui::DragFloat("Head Height", &hb->headHeight, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Head Radius", &hb->headRadius, 0.01f, 0.01f, 5.0f);
                ImGui::DragFloat("Head Multiplier", &hb->headMultiplier, 0.05f, 1.0f, 10.0f);
            }
            ImGui::PopID();
        }

        if (auto* sp = e->GetComponent<SpawnerComponent>()) {
            ImGui::PushID("cmp_spawner");
            ImGui::BulletText("Spawner (waves)"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<SpawnerComponent>();
            else {
                editStr("Prefab", sp->prefabPath);
                if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の Prefab をここへドロップ
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PREFAB"))
                        if (pl->Data) sp->prefabPath = static_cast<const char*>(pl->Data);
                    ImGui::EndDragDropTarget();
                }
                ImGui::DragInt("Waves", &sp->waveCount, 0.1f, 1, 99);
                ImGui::DragInt("First Wave Count", &sp->baseCount, 0.1f, 1, 100);
                ImGui::DragInt("Increase / Wave", &sp->countIncrease, 0.1f, 0, 50);
                ImGui::DragFloat("Spawn Interval", &sp->spawnInterval, 0.05f, 0.05f, 10.0f);
                ImGui::DragFloat("First Delay", &sp->firstDelay, 0.05f, 0.0f, 30.0f);
                ImGui::DragFloat("Wave Delay", &sp->waveDelay, 0.05f, 0.0f, 30.0f);
                ImGui::DragFloat("Spawn Radius", &sp->spawnRadius, 0.05f, 0.0f, 50.0f);
            }
            ImGui::PopID();
        }

        if (auto* shc = e->GetComponent<ShaderComponent>()) {
            ImGui::PushID("cmp_shader");
            ImGui::BulletText("ShaderComponent"); ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) e->RemoveComponent<ShaderComponent>();
            else {
                ShadingParams& p = shc->params;
                const char* modeNames[] = { "Lit", "Unlit", "Toon" };   // same order as ShadingMode
                int mode = (int)p.mode;
                if (ImGui::Combo("Mode", &mode, modeNames, 3)) p.mode = (ShadingMode)mode;
                if (p.mode != ShadingMode::Unlit) {   // Unlit has no lighting, so these do nothing there
                    ImGui::Checkbox("Receive Shadow", &p.receiveShadow);
                    if (p.mode == ShadingMode::Toon) {
                        ImGui::DragFloat("Toon Threshold", &p.toonThreshold, 0.005f, 0.0f, 1.0f);
                        ImGui::DragFloat("Toon Softness", &p.toonSoftness, 0.002f, 0.001f, 0.5f);
                        ImGui::ColorEdit3("Shade Color", &p.toonShadeColor.x);
                        // Texture simplification. 0 = off for both.
                        ImGui::DragFloat("Texture Steps", &p.toonTexSteps, 0.05f, 0.0f, 16.0f, "%.0f");
                        ImGui::DragFloat("Texture Smooth", &p.toonTexSmooth, 0.0005f, 0.0f, 0.05f, "%.4f");
                    }
                    ImGui::DragFloat("Metallic", &p.metallic, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Roughness", &p.roughness, 0.005f, 0.05f, 1.0f);
                    ImGui::DragFloat("Specular Strength", &p.specularStrength, 0.01f, 0.0f, 4.0f);
                    ImGui::DragFloat("Rim Strength", &p.rimStrength, 0.01f, 0.0f, 4.0f);
                    ImGui::DragFloat("Rim Power", &p.rimPower, 0.05f, 0.5f, 16.0f);
                    ImGui::ColorEdit3("Rim Color", &p.rimColor.x);
                }
                // Outline works in every mode. Width 0 = off.
                ImGui::DragFloat("Outline Width", &p.outlineWidth, 0.001f, 0.0f, 0.2f, "%.3f");
                ImGui::ColorEdit3("Outline Color", &p.outlineColor.x);
            }
            ImGui::PopID();
        }
        ImGui::End();
    }

    void HandleShortcuts(CommandHistory& history)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput) return;
        if (io.KeyCtrl && ImGui::IsKeyPressed('Z')) history.Undo();
        if (io.KeyCtrl && ImGui::IsKeyPressed('Y')) history.Redo();
    }

    void DrawMainMenuBar(World& world, EntityId& selected, CommandHistory& history,
        bool playing, bool& togglePlayRequested,
        const std::vector<std::string>& sceneNames, int& activeScene, bool& addSceneRequested,
        std::string& renameSceneRequested, bool& sceneSaved)
    {
        static char sceneRenameBuf[64] = "";
        // 現在のSceneごとに保存先を変える
        std::string kPath = (activeScene >= 0 && activeScene < (int)sceneNames.size())
            ? (sceneNames[activeScene] + ".json") : std::string("Stage.json");
        bool doSave = false, doLoad = false;

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) doSave = true;
                if (ImGui::MenuItem("Load Scene", "Ctrl+L")) doLoad = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, history.CanUndo())) history.Undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, history.CanRedo())) history.Redo();
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(playing ? "Stop" : "Play")) togglePlayRequested = true;

            // --- Scene 切替 ---
            ImGui::Separator();
            ImGui::TextUnformatted("Scene:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            const char* cur = (activeScene >= 0 && activeScene < (int)sceneNames.size()) ? sceneNames[activeScene].c_str() : "";
            if (ImGui::BeginCombo("##scene", cur)) {
                for (int i = 0; i < (int)sceneNames.size(); ++i)
                    if (ImGui::Selectable(sceneNames[i].c_str(), i == activeScene)) activeScene = i;
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("+Scene")) addSceneRequested = true;
            ImGui::SameLine();
            if (!playing && ImGui::SmallButton("Rename")) {
                strncpy_s(sceneRenameBuf, sizeof(sceneRenameBuf), cur, _TRUNCATE);
                ImGui::OpenPopup("RenameScenePopup");
            }
            if (ImGui::BeginPopup("RenameScenePopup")) {
                ImGui::TextUnformatted("New scene name");
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                bool ok = ImGui::InputText("##scenename", sceneRenameBuf, sizeof(sceneRenameBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue);
                if (ImGui::Button("OK")) ok = true;
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                if (ok) { renameSceneRequested = sceneRenameBuf; ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
            ImGui::EndMainMenuBar();
        }

        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && io.KeyCtrl) {
            if (ImGui::IsKeyPressed('S')) doSave = true;
            if (ImGui::IsKeyPressed('L')) doLoad = true;
        }
        if (doSave) {
            StageSerializer::Save(world, kPath.c_str());
            StageSerializer::SaveSceneList(sceneNames, "Scenes.json");   // ★Scene一覧も保存
            sceneSaved = true;   // Engine saves Environment.json when it sees this
        }
        if (doLoad && StageSerializer::Load(world, kPath.c_str())) { selected = EntityId{}; history.Clear(); }
    }

    void DrawAssetBrowserPanel(World& world, EntityId& selected, CommandHistory& history)
    {
        // dir -> [fullpath...]
        static std::map<std::string, std::vector<std::string>> groups;
        static int   total = 0;
        static bool  scanned = false;
        static char  search[128] = "";
        static std::string picked;

        ImGui::Begin("Asset Browser");

        bool rescanClicked = ImGui::Button("Rescan");
        if (!scanned || rescanClicked || g_assetRescan)
        {
            g_assetRescan = false;
            groups.clear();
            total = 0;
            std::error_code ec;
            std::filesystem::path root = kAssetRoot;
            if (std::filesystem::exists(root, ec)) {
                try {
                    for (auto& entry : std::filesystem::recursive_directory_iterator(
                        root, std::filesystem::directory_options::skip_permission_denied)) {
                        if (!entry.is_regular_file()) continue;
                        std::string ext = entry.path().extension().string();
                        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
                        std::string fname = entry.path().generic_string();
                        if (ext != ".glb" && !IsPrefabPath(fname) && !IsAnimGraphPath(fname)) continue;
                        std::string full = entry.path().generic_string();
                        std::string dir = full.substr(0, full.find_last_of('/'));
                        groups[dir].push_back(full);
                        ++total;
                    }
                }
                catch (...) {}
            }
            scanned = true;
        }
        ImGui::SameLine();
        ImGui::Text("%d assets", total);

        ImGui::InputTextWithHint("##search", "search...", search, sizeof(search));

        bool hasPick = !picked.empty();
        bool pickIsPrefab = hasPick && IsPrefabPath(picked);
        bool pickIsGraph = hasPick && IsAnimGraphPath(picked);
        // Prefab とグラフは、メッシュとして割り当てない
        if (ImGui::Button("Assign to Selected") && hasPick && !pickIsPrefab && !pickIsGraph) {
            if (Entity* e = world.Get(selected)) {
                auto* m = e->GetComponent<MeshRendererComponent>();
                if (!m) m = e->AddComponent<MeshRendererComponent>();
                m->assetId = picked;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Entity") && hasPick && !pickIsGraph) {    // グラフからは Entity を作らない
            if (pickIsPrefab) {
                EntityId id = EditorSpawnPrefab(world, history, picked);
                if (id.IsValid()) selected = id;
            }
            else {
                std::string name = picked.substr(picked.find_last_of('/') + 1);
                EntityId id = EditorCreateEntity(world, history, name.c_str());
                if (Entity* e = world.Get(id))
                    if (auto* m = e->GetComponent<MeshRendererComponent>())
                        m->assetId = picked;
                selected = id;
            }
        }

        ImGui::Separator();
        ImGui::BeginChild("assetlist");

        const bool filtering = (search[0] != '\0');
        for (auto& kv : groups)
        {
            const std::string& dir = kv.first;
            const auto& list = kv.second;

            // 検索中：一致ファイルが無いフォルダは非表示、あるフォルダは自動展開
            if (filtering) {
                bool any = false;
                for (const auto& f : list) {
                    std::string n = f.substr(f.find_last_of('/') + 1);
                    if (n.find(search) != std::string::npos) { any = true; break; }
                }
                if (!any) continue;
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            }

            // フォルダ名はルート以下の相対で表示（見やすさ）
            if (ImGui::TreeNode(dir.c_str()))
            {
                for (const auto& f : list)
                {
                    std::string name = f.substr(f.find_last_of('/') + 1);
                    if (filtering && name.find(search) == std::string::npos) continue;

                    // 種類ごとに、名前の前の印と、ドラッグで運ぶデータの種類を変える
                    const bool prefab = IsPrefabPath(f);
                    const bool graph = IsAnimGraphPath(f);
                    const char* tag = prefab ? "[Prefab] " : (graph ? "[Graph] " : "");
                    const char* payloadType = prefab ? "ASSET_PREFAB" : (graph ? "ASSET_ANIMGRAPH" : "ASSET_GLB");
                    char item[320];
                    snprintf(item, sizeof(item), "%s%s##%s", tag, name.c_str(), f.c_str());
                    if (ImGui::Selectable(item, f == picked))
                        picked = f;

                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload(payloadType, f.c_str(), f.size() + 1);
                        ImGui::TextUnformatted(name.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
                ImGui::TreePop();
            }
        }

        ImGui::EndChild();
        ImGui::End();
    }


    void DrawEnvironmentPanel(SceneLighting& light, PostProcessSettings& post)
    {
        ImGui::Begin("Environment");
        ImGui::DragFloat3("Light Dir", &light.direction.x, 0.02f, -1.0f, 1.0f);
        ImGui::ColorEdit3("Light Color", &light.color.x);
        ImGui::DragFloat("Ambient", &light.ambient, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Shadow Range", &light.shadowRange, 0.5f, 10.0f, 120.0f, "%.0f m");

        ImGui::Separator();
        ImGui::TextUnformatted("Post Process");
        ImGui::DragFloat("Saturation", &post.saturation, 0.01f, 0.0f, 2.0f);
        const char* viewNames[] = { "Final", "Normal", "Depth", "Edge" };   // same order as PostDebugView
        int view = (int)post.debugView;
        if (ImGui::Combo("View", &view, viewNames, 4)) post.debugView = (PostDebugView)view;

        ImGui::Checkbox("Edge Outline", &post.edgeEnabled);
        if (post.edgeEnabled || post.debugView == PostDebugView::Edge) {
            ImGui::ColorEdit3("Edge Color", post.edgeColor);
            ImGui::DragFloat("Edge Thickness", &post.edgeThickness, 0.05f, 0.5f, 4.0f);
            ImGui::DragFloat("Depth Threshold", &post.edgeDepthThreshold, 0.001f, 0.001f, 1.0f, "%.3f");
            ImGui::DragFloat("Normal Threshold", &post.edgeNormalThreshold, 0.005f, 0.01f, 2.0f);
            ImGui::DragFloat("Fade Distance", &post.edgeFadeDistance, 0.5f, 1.0f, 100.0f);
        }

        ImGui::Separator();
        ImGui::TextDisabled("Saved to Environment.json by Save Scene (Ctrl+S)");
        ImGui::End();
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


    void DrawNavGridPanel(World& world)
    {
        static EngineNavGrid nav;
        static float cell = 1.0f;
        static int   cols = 40, rows = 40;
        static float center[3] = { 0.0f, 0.0f, 0.0f };

        ImGui::Begin("NavGrid");
        ImGui::SetNextItemWidth(120); ImGui::DragFloat("CellSize", &cell, 0.05f, 0.1f, 10.0f);
        ImGui::SameLine(); ImGui::SetNextItemWidth(90); ImGui::DragInt("Cols", &cols, 1, 4, 256);
        ImGui::SameLine(); ImGui::SetNextItemWidth(90); ImGui::DragInt("Rows", &rows, 1, 4, 256);
        ImGui::DragFloat3("Center(XYZ)", center, 0.1f);

        nav.Configure({ center[0], center[1], center[2] }, cell, cols, rows);
        nav.Bake(world);

        // Player(goal) と 最初のOpponent(start) を探す
        Entity* player = nullptr; Entity* opp = nullptr;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            if (!player && slot->GetComponent<PlayerComponent>()) player = slot.get();
            if (!opp && slot->GetComponent<OpponentComponent>()) opp = slot.get();
        }
        std::vector<DirectX::XMFLOAT3> path;
        if (player && opp) {
            auto* pt = player->GetComponent<TransformComponent>();
            auto* ot = opp->GetComponent<TransformComponent>();
            if (pt && ot) nav.FindPath(ot->position, pt->position, path);
        }
        ImGui::Text("path waypoints: %d", (int)path.size());

        // トップダウン描画
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float px = min(avail.x / cols, avail.y / rows); if (px < 2.0f) px = 2.0f;
        ImGui::InvisibleButton("navcanvas", ImVec2(px * cols, px * rows));

        auto cellRect = [&](int cx, int cz, ImU32 col) {
            ImVec2 a(p0.x + cx * px, p0.y + cz * px);
            dl->AddRectFilled(a, ImVec2(a.x + px - 1, a.y + px - 1), col);
            };
        for (int z = 0; z < rows; ++z)
            for (int x = 0; x < cols; ++x)
                cellRect(x, z, nav.Blocked(x, z) ? IM_COL32(180, 60, 60, 255) : IM_COL32(40, 44, 52, 255));

        auto worldToPix = [&](DirectX::XMFLOAT3 w) {
            int cx, cz; nav.WorldToCell(w, cx, cz);
            return ImVec2(p0.x + (cx + 0.5f) * px, p0.y + (cz + 0.5f) * px);
            };
        for (size_t i = 1; i < path.size(); ++i)
            dl->AddLine(worldToPix(path[i - 1]), worldToPix(path[i]), IM_COL32(255, 220, 60, 255), 2.0f);
        if (opp) if (auto* ot = opp->GetComponent<TransformComponent>())    dl->AddCircleFilled(worldToPix(ot->position), 5, IM_COL32(255, 140, 40, 255));
        if (player) if (auto* pt = player->GetComponent<TransformComponent>()) dl->AddCircleFilled(worldToPix(pt->position), 5, IM_COL32(60, 220, 90, 255));

        ImGui::End();
    }

} // namespace uel