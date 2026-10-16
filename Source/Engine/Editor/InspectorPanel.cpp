#include "Engine/Editor/EditorPanels.h"
#include "Engine/Editor/TransformEdit.h"
#include "Engine/Editor/EntityCommands.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Components/SkinnedMeshComponent.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Components/PrefabInstanceComponent.h"
#include "Engine/Components/ShaderComponent.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Components/PlayerComponent.h"
#include "Engine/Components/OpponentComponent.h"
#include "Engine/Components/ScreenUIComponent.h"
#include "Engine/Components/GameRuleComponent.h"
#include "Engine/Components/HitboxComponent.h"
#include "Engine/Components/SpawnerComponent.h"
#include "Engine/Components/NavObstacleComponent.h"
#include "Engine/Animation/AnimStateMachineNode.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Serialization/StageSerializer.h"
#include "Engine/Serialization/AnimGraphSerializer.h"
#include "Engine/Config/EngineConfig.h"

#include <imgui.h>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Inspector パネル（選択中の Entity の名前・Prefab・各 Component を編集する）。
namespace uel {

    namespace {
        std::string g_prefabRefresh;         // Apply されたPrefab（Engineが全Sceneへ反映）
    }

    std::string ConsumePrefabRefreshRequest()
    {
        std::string p = g_prefabRefresh;
        g_prefabRefresh.clear();
        return p;
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
                RequestAssetRescan();
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

                // ---- 一人称で潰す骨（Play 中、プレイヤーのときだけ効く）----
                {
                    std::shared_ptr<SkinnedRenderModel> fm;
                    if (!sk->assetPath.empty()) fm = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
                    const bool haveBones = fm && fm->Valid();
                    if (ImGui::BeginCombo("FP Hide Bone", sk->fpHideBone.empty() ? "(none)" : sk->fpHideBone.c_str())) {
                        if (ImGui::Selectable("(none)", sk->fpHideBone.empty())) sk->fpHideBone.clear();
                        if (haveBones) {
                            const auto& bones = fm->Cpu().skeleton.bones;
                            for (int i = 0; i < (int)bones.size(); ++i) {
                                ImGui::PushID(i);
                                if (ImGui::Selectable(bones[i].name.c_str(), bones[i].name == sk->fpHideBone)) sk->fpHideBone = bones[i].name;
                                ImGui::PopID();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::SmallButton("Auto (head)") && haveBones) {
                        // 名前に "head" を含む最初の骨を選ぶ（骨は親が先に並んでいるので、いちばん根元に近いものになる）
                        for (const auto& b : fm->Cpu().skeleton.bones) {
                            std::string low = b.name;
                            for (char& ch : low) ch = (char)std::tolower((unsigned char)ch);
                            if (low.find("head") != std::string::npos) { sk->fpHideBone = b.name; break; }
                        }
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("Preview##fp", &sk->firstPerson);      // 編集中に結果を見るためのもの（保存されない）
                }
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
                        if (dynamic_cast<AnimStateMachineNode*>(an->graph.get())) {
                            // ステートマシンは、ノードエディタで編集する
                            ImGui::TextDisabled("Edit states and transitions in the Anim Graph panel.");
                        }
                        else {
                            ImGui::PushID("graph");     // グラフの中の部品が、下の Parameters の部品と同じ ID にならないように分ける
                            DrawAnimNodeDetails(an->graph.get(), amodel->Cpu(), an->params);
                            ImGui::PopID();
                        }
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

                    ImGui::TextDisabled("To use a graph: right-click the canvas in the Anim Graph panel.");

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
                                const bool ok = LoadAnimGraph(path, an->params, an->graph, an->montages);
                                if (ok) an->graphPath = path;
                                graphMsg = ok ? "Loaded" : "Load failed (file?)";
                                graphMsgTimer = 3.0f;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (ImGui::SmallButton("Save Graph")) {
                        const std::string path = AnimGraphPathFromName(an->graphPath);
                        const bool ok = an->graph && !path.empty() && SaveAnimGraph(path, an->params, an->graph.get(), an->montages);
                        if (ok) { an->graphPath = path; RequestAssetRescan(); }
                        graphMsg = ok ? "Saved" : (an->graph ? "Save failed (name?)" : "No graph to save");
                        graphMsgTimer = 3.0f;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Load Graph")) {
                        const std::string path = AnimGraphPathFromName(an->graphPath);
                        const bool ok = !path.empty() && LoadAnimGraph(path, an->params, an->graph, an->montages);
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

} // namespace uel
