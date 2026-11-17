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
#include "Engine/Components/AllyComponent.h"
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
#include "Engine/Components/PartySetupComponent.h"
#include "Engine/Components/GuideComponent.h"
#include "Engine/Components/GuidePointComponent.h"

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
        if (!e) { ImGui::TextUnformatted(u8"（何も選んでいません）"); ImGui::End(); return; }

        {
            static std::string renameOld;
            std::string before = e->name;
            char nameBuf[128]; strncpy_s(nameBuf, sizeof(nameBuf), e->name.c_str(), _TRUNCATE);
            if (ImGui::InputText(u8"名前", nameBuf, sizeof(nameBuf))) e->name = nameBuf;
            if (ImGui::IsItemActivated()) renameOld = before;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                std::string newName = e->name;
                e->name = renameOld;                                   // いったん戻してから
                EditorRenameEntity(world, history, selected, renameOld, newName);   // Undo付きで適用
            }
        }
        ImGui::Text(u8"ID：%u（世代 %u）", e->id.index, e->id.generation);
        {
            static std::string prefabMsg;
            static float       prefabMsgTimer = 0.0f;

            if (auto* pi = e->GetComponent<PrefabInstanceComponent>()) {
                // ---- Prefabのインスタンス ----
                std::string ppath = pi->path;   // Apply/Revertで部品が作り直されるので先にコピー
                ImGui::TextColored(ImVec4(0.45f, 0.70f, 1.0f, 1.0f), u8"Prefab：%s", ppath.c_str());
                if (ImGui::Button(u8"Prefab に反映")) {
                    bool ok = StageSerializer::SavePrefab(*e, ppath.c_str());
                    if (ok) g_prefabRefresh = ppath;                   // 全Sceneの同じPrefabへ反映
                    prefabMsg = ok ? u8"反映しました" : u8"反映に失敗しました";
                    prefabMsgTimer = 3.0f;
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"元に戻す")) {
                    bool ok = StageSerializer::RevertPrefabInstance(*e);
                    prefabMsg = ok ? u8"元に戻しました" : u8"戻せませんでした（Prefab が見つからない？）";
                    prefabMsgTimer = 3.0f;
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"リンクを解除")) e->RemoveComponent<PrefabInstanceComponent>();
                ImGui::TextDisabled(u8"個別に持てるのは名前と Transform だけ。ほかの変更は「Prefab に反映」で保存。");
            }
            else if (ImGui::Button(u8"Prefab として保存")) {
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
                prefabMsg = ok ? (u8"保存しました：" + path) : std::string(u8"保存に失敗しました");
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
            ImGui::DragFloat3(u8"位置", &t->position.x, 0.05f);
            if (ImGui::IsItemActivated())            BeginTransformEdit(session, selected, *t);
            if (ImGui::IsItemDeactivatedAfterEdit()) CommitTransformEdit(session, world, history);
            ImGui::DragFloat3(u8"回転", &t->rotationEuler.x, 0.5f);
            if (ImGui::IsItemActivated())            BeginTransformEdit(session, selected, *t);
            if (ImGui::IsItemDeactivatedAfterEdit()) CommitTransformEdit(session, world, history);
            ImGui::DragFloat3(u8"スケール", &t->scale.x, 0.05f, 0.001f, 1000.0f);
            if (ImGui::IsItemActivated())            BeginTransformEdit(session, selected, *t);
            if (ImGui::IsItemDeactivatedAfterEdit()) CommitTransformEdit(session, world, history);
        }

        if (MeshRendererComponent* m = e->GetComponent<MeshRendererComponent>())
        {
            ImGui::PushID("cmp_meshrenderer");
            ImGui::Separator(); ImGui::TextUnformatted("MeshRenderer"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<MeshRendererComponent>();
            else {
                char buf[260]; strncpy_s(buf, sizeof(buf), m->assetId.c_str(), _TRUNCATE);
                if (ImGui::InputText(u8"モデル（GLB）", buf, sizeof(buf))) m->assetId = buf;
                if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の GLB をここへドロップ
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_GLB"))
                        if (pl->Data) m->assetId = static_cast<const char*>(pl->Data);
                    ImGui::EndDragDropTarget();
                }
                ImGui::Checkbox(u8"表示", &m->visible);
                ImGui::ColorEdit4(u8"色", &m->color.x);

                // サブメッシュ：役割割当＋可視（このインスタンス）
                if (!m->assetId.empty()) {
                    std::shared_ptr<Mesh> mesh = cache.GetOrLoad(device, m->assetId);
                    if (mesh && mesh->IsValid()) {
                        const auto& subs = mesh->GetSubmeshes();
                        if (m->hiddenSubmesh.size() < subs.size()) m->hiddenSubmesh.resize(subs.size(), 0);
                        if (m->submeshRole.size() < subs.size()) m->submeshRole.resize(subs.size(), 0);

                        ImGui::Separator();
                        ImGui::TextUnformatted(u8"サブメッシュと役割（この Entity だけ）");
                        ImGui::Checkbox(u8"一人称表示（「一人称で隠す」を表示しない）", &m->firstPerson);

                        if (ImGui::Button(u8"頭を自動で判定")) {
                            for (size_t i = 0; i < subs.size(); ++i) {
                                std::string low = subs[i].name;
                                for (char& ch : low) ch = (char)std::tolower((unsigned char)ch);
                                bool headLike = (low.find("head") != std::string::npos)
                                    || (low.find("face") != std::string::npos)
                                    || (low.find("skull") != std::string::npos);
                                m->submeshRole[i] = headLike ? (uint8_t)1 : (uint8_t)0;
                            }
                        }

                        const char* roleNames[] = { u8"なし", u8"一人称で隠す" };
                        for (size_t i = 0; i < subs.size(); ++i) {
                            ImGui::PushID((int)i);
                            bool vis = (m->hiddenSubmesh[i] == 0);
                            if (ImGui::Checkbox("##vis", &vis)) m->hiddenSubmesh[i] = vis ? 0 : 1;
                            ImGui::SameLine();
                            int role = m->submeshRole[i];
                            ImGui::SetNextItemWidth(110);
                            if (ImGui::Combo("##role", &role, roleNames, 2)) m->submeshRole[i] = (uint8_t)role;
                            ImGui::SameLine();
                            std::string nm = subs[i].name.empty() ? (u8"サブメッシュ " + std::to_string(i)) : subs[i].name;
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
            ImGui::Separator(); ImGui::TextUnformatted(u8"SkinnedMesh（アニメ付きメッシュ）"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<SkinnedMeshComponent>();
            else {
                char sbuf[260]; strncpy_s(sbuf, sizeof(sbuf), sk->assetPath.c_str(), _TRUNCATE);
                if (ImGui::InputText(u8"モデル（GLB）", sbuf, sizeof(sbuf))) sk->assetPath = sbuf;
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

                    ImGui::TextUnformatted(u8"追加のアニメ（GLB）");
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
                            if ((*counts)[i] > 0) ImGui::TextDisabled(u8"+%d クリップ", (*counts)[i]);
                            else ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), u8"+0（骨が合わない、またはクリップ名が重複）");
                        }
                        ImGui::PopID();
                    }
                    bool animChanged = false;
                    if (removeAnim >= 0) { animFiles.erase(animFiles.begin() + removeAnim); animChanged = true; }

                    ImGui::Button(u8"ここにアニメの GLB をドロップして追加", ImVec2(-1.0f, 0.0f));         // ドロップの受け取り先（押しても何もしない）
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

                ImGui::Checkbox(u8"表示##sk", &sk->visible);
                ImGui::ColorEdit4(u8"色##sk", &sk->color.x);

                // ---- 一人称で潰す骨（Play 中、プレイヤーのときだけ効く）----
                {
                    std::shared_ptr<SkinnedRenderModel> fm;
                    if (!sk->assetPath.empty()) fm = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
                    const bool haveBones = fm && fm->Valid();
                    if (ImGui::BeginCombo(u8"一人称で隠す骨", sk->fpHideBone.empty() ? u8"（なし）" : sk->fpHideBone.c_str())) {
                        if (ImGui::Selectable(u8"（なし）", sk->fpHideBone.empty())) sk->fpHideBone.clear();
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
                    if (ImGui::SmallButton(u8"自動（頭）") && haveBones) {
                        // 名前に "head" を含む最初の骨を選ぶ（骨は親が先に並んでいるので、いちばん根元に近いものになる）
                        for (const auto& b : fm->Cpu().skeleton.bones) {
                            std::string low = b.name;
                            for (char& ch : low) ch = (char)std::tolower((unsigned char)ch);
                            if (low.find("head") != std::string::npos) { sk->fpHideBone = b.name; break; }
                        }
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox(u8"確認用に表示##fp", &sk->firstPerson);      // 編集中に結果を見るためのもの（保存されない）
                }
                if (!sk->assetPath.empty()) {
                    auto model = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
                    if (model && model->Valid()) {
                        const auto& clips = model->Cpu().clips;
                        ImGui::Text(u8"骨：%d　クリップ：%d",
                            (int)model->Cpu().skeleton.bones.size(), (int)clips.size());
                        if (e->GetComponent<AnimatorComponent>()) {
                            // 再生の状態は AnimatorComponent が持つので、ここでは編集しない
                            ImGui::TextDisabled(u8"クリップと再生位置は AnimatorComponent が動かしています。");
                        }
                        else if (!clips.empty()) {
                            if (sk->clipIndex < 0 || sk->clipIndex >= (int)clips.size()) sk->clipIndex = 0;
                            const std::string& preview = clips[sk->clipIndex].name;
                            if (ImGui::BeginCombo(u8"クリップ", preview.c_str())) {
                                for (int i = 0; i < (int)clips.size(); ++i) {
                                    bool seld = (i == sk->clipIndex);
                                    std::string label = clips[i].name.empty() ? (u8"クリップ " + std::to_string(i)) : clips[i].name;
                                    if (ImGui::Selectable(label.c_str(), seld)) { sk->clipIndex = i; sk->time = 0.0f; }
                                    if (seld) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            float dur = clips[sk->clipIndex].duration;
                            ImGui::Checkbox(u8"再生", &sk->playing); ImGui::SameLine();
                            ImGui::Checkbox(u8"ループ", &sk->loop);
                            ImGui::DragFloat(u8"速度", &sk->speed, 0.01f, -4.0f, 4.0f);
                            ImGui::SliderFloat(u8"再生位置", &sk->time, 0.0f, dur > 0.0f ? dur : 1.0f, u8"%.3f 秒");
                        }
                        else {
                            ImGui::TextDisabled(u8"このモデルにはクリップがありません。");
                        }
                    }
                    else {
                        ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), u8"読み込みに失敗しました");
                    }
                }
            }
            ImGui::PopID();
        }

        // ---- 追加・削除できる Component ----
        // 並び：見た目・アニメ（Animator / Shader）→ ゲーム（Player 〜 ScreenUI）。一番下の「+ コンポーネントを追加」のメニューも同じ順
        ImGui::Separator();
        ImGui::TextUnformatted(u8"コンポーネント");

        auto editStr = [](const char* label, std::string& s) {
            char b[256]; strncpy_s(b, sizeof(b), s.c_str(), _TRUNCATE);
            if (ImGui::InputText(label, b, sizeof(b))) s = b;
            };

        // AnimatorComponent（クリップの切り替えとクロスフェード）
        if (auto* an = e->GetComponent<AnimatorComponent>()) {
            ImGui::PushID("cmp_animator");
            ImGui::BulletText("AnimatorComponent"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<AnimatorComponent>();
            else {
                SkinnedMeshComponent* ask = e->GetComponent<SkinnedMeshComponent>();
                std::shared_ptr<SkinnedRenderModel> amodel;
                if (ask && !ask->assetPath.empty()) amodel = GlobalSkinnedCache().GetOrLoad(device, ask->assetPath, ask->animPath);
                const bool hasClips = amodel && amodel->Valid() && !amodel->Cpu().clips.empty();

                if (!ask) {
                    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), u8"同じ Entity に SkinnedMeshComponent が必要です。");
                }
                else if (!hasClips) {
                    ImGui::TextDisabled(u8"再生できるクリップがありません。");
                }
                else if (an->graph) {
                    // ---- グラフで動かしているとき ----
                    ImGui::Text(u8"グラフの根：%s ノード", an->graph->TypeName());
                    ImGui::SameLine();
                    if (ImGui::SmallButton(u8"グラフを消す")) an->graph.reset();
                    if (an->graph) {
                        if (dynamic_cast<AnimStateMachineNode*>(an->graph.get())) {
                            // ステートマシンは、ノードエディタで編集する
                            ImGui::TextDisabled(u8"状態と遷移は Anim Graph パネルで編集します。");
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
                    if (ImGui::BeginCombo(u8"クリップ", preview.c_str())) {
                        for (int i = 0; i < (int)clips.size(); ++i) {
                            bool seld = (i == an->clipIndex);
                            std::string label = clips[i].name.empty() ? (u8"クリップ " + std::to_string(i)) : clips[i].name;
                            if (ImGui::Selectable(label.c_str(), seld)) an->Play(i);      // Blend Time かけて切り替わる
                            if (seld) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::DragFloat(u8"ブレンド時間", &an->blendTime, 0.01f, 0.0f, 2.0f, u8"%.2f 秒");
                    ImGui::Checkbox(u8"ループ", &an->loop);
                    if (an->IsFading()) ImGui::Text(u8"ブレンド中　%.0f%%", an->fadeElapsed / an->fadeDuration * 100.0f);
                    else                ImGui::TextDisabled(u8"ブレンド中　-");

                    ImGui::TextDisabled(u8"グラフを使うには：Anim Graph パネルのキャンバスを右クリック。");

                }
                // ---- グラフのアセット（Data/Anim/<名前>.animgraph.json）----
                {
                    static std::string graphMsg;
                    static float       graphMsgTimer = 0.0f;
                    char gb[200]; strncpy_s(gb, sizeof(gb), an->graphPath.c_str(), _TRUNCATE);
                    if (ImGui::InputText(u8"グラフのファイル", gb, sizeof(gb))) an->graphPath = gb;     // 名前だけでもよい（例：Enemy）
                    if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の [Graph] をここへドロップすると読み込む
                        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_ANIMGRAPH")) {
                            if (pl->Data) {
                                const std::string path = static_cast<const char*>(pl->Data);
                                const bool ok = LoadAnimGraph(path, an->params, an->graph, an->montages, an->aim);
                                if (ok) an->graphPath = path;
                                graphMsg = ok ? u8"読み込みました" : u8"読み込めませんでした（ファイル？）";
                                graphMsgTimer = 3.0f;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (ImGui::SmallButton(u8"グラフを保存")) {
                        const std::string path = AnimGraphPathFromName(an->graphPath);
                        const bool ok = an->graph && !path.empty() && SaveAnimGraph(path, an->params, an->graph.get(), an->montages, an->aim);
                        if (ok) { an->graphPath = path; RequestAssetRescan(); }
                        graphMsg = ok ? u8"保存しました" : (an->graph ? u8"保存できませんでした（名前？）" : u8"保存するグラフがありません");
                        graphMsgTimer = 3.0f;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton(u8"グラフを読み込む")) {
                        const std::string path = AnimGraphPathFromName(an->graphPath);
                        const bool ok = !path.empty() && LoadAnimGraph(path, an->params, an->graph, an->montages, an->aim);
                        if (ok) an->graphPath = path;
                        graphMsg = ok ? u8"読み込みました" : u8"読み込めませんでした（ファイル？）";
                        graphMsgTimer = 3.0f;
                    }
                    if (graphMsgTimer > 0.0f) {
                        graphMsgTimer -= ImGui::GetIO().DeltaTime;
                        ImGui::SameLine(); ImGui::TextUnformatted(graphMsg.c_str());
                    }
                    ImGui::TextDisabled(u8"グラフの変更は「グラフを保存」を押すまで残りません。");
                }
                ImGui::DragFloat(u8"再生速度（全体）", &an->speed, 0.01f, -4.0f, 4.0f);
                ImGui::Checkbox(u8"再生", &an->playing);

                // ---- パラメータ（グラフに渡す値）----
                ImGui::Separator();
                ImGui::TextUnformatted(u8"パラメータ");
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
                        if (ImGui::SmallButton(u8"発火")) p.value = 1.0f;
                        ImGui::SameLine(); ImGui::TextDisabled(p.value > 0.5f ? u8"（オン）" : "(-)");
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x")) removeAt = i;
                    ImGui::PopID();
                }
                if (removeAt >= 0) an->params.items.erase(an->params.items.begin() + removeAt);
                if (ImGui::SmallButton(u8"+ パラメータ")) {
                    AnimParam p; p.name = "Param" + std::to_string(an->params.items.size());
                    an->params.items.push_back(p);
                }
            }
            ImGui::PopID();
        }

        // ShaderComponent（描き方・材質）
        if (auto* shc = e->GetComponent<ShaderComponent>()) {
            ImGui::PushID("cmp_shader");
            ImGui::BulletText("ShaderComponent"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<ShaderComponent>();
            else {
                ShadingParams& p = shc->params;
                const char* modeNames[] = { "Lit", "Unlit", "Toon" };   // same order as ShadingMode
                int mode = (int)p.mode;
                if (ImGui::Combo(u8"描き方", &mode, modeNames, 3)) p.mode = (ShadingMode)mode;
                if (p.mode != ShadingMode::Unlit) {   // Unlit has no lighting, so these do nothing there
                    ImGui::Checkbox(u8"影を受ける", &p.receiveShadow);
                    if (p.mode == ShadingMode::Toon) {
                        ImGui::DragFloat(u8"トゥーンの境目", &p.toonThreshold, 0.005f, 0.0f, 1.0f);
                        ImGui::DragFloat(u8"トゥーンのぼかし", &p.toonSoftness, 0.002f, 0.001f, 0.5f);
                        ImGui::ColorEdit3(u8"陰の色", &p.toonShadeColor.x);
                        // Texture simplification. 0 = off for both.
                        ImGui::DragFloat(u8"テクスチャの段階数", &p.toonTexSteps, 0.05f, 0.0f, 16.0f, "%.0f");
                        ImGui::DragFloat(u8"テクスチャのなめらかさ", &p.toonTexSmooth, 0.0005f, 0.0f, 0.05f, "%.4f");
                    }
                    ImGui::DragFloat(u8"金属っぽさ", &p.metallic, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat(u8"粗さ", &p.roughness, 0.005f, 0.05f, 1.0f);
                    ImGui::DragFloat(u8"ハイライトの強さ", &p.specularStrength, 0.01f, 0.0f, 4.0f);
                    ImGui::DragFloat(u8"リムライトの強さ", &p.rimStrength, 0.01f, 0.0f, 4.0f);
                    ImGui::DragFloat(u8"リムライトの鋭さ", &p.rimPower, 0.05f, 0.5f, 16.0f);
                    ImGui::ColorEdit3(u8"リムライトの色", &p.rimColor.x);
                }
                // Outline works in every mode. Width 0 = off.
                ImGui::DragFloat(u8"輪郭線の太さ", &p.outlineWidth, 0.001f, 0.0f, 0.2f, "%.3f");
                ImGui::ColorEdit3(u8"輪郭線の色", &p.outlineColor.x);
            }
            ImGui::PopID();
        }

        // PlayerComponent
        if (auto* pc = e->GetComponent<PlayerComponent>()) {
            ImGui::PushID("cmp_player");
            ImGui::BulletText("PlayerComponent"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<PlayerComponent>();
            else {
                ImGui::DragFloat(u8"移動速度##pl", &pc->moveSpeed, 0.1f, 0.0f, 50.0f);
                ImGui::DragFloat(u8"目の高さ", &pc->eyeHeight, 0.05f, 0.0f, 20.0f);
                ImGui::DragFloat(u8"目の前方へのずれ", &pc->eyeForward, 0.05f, 0.0f, 10.0f);
            }
            ImGui::PopID();
        }

        // OpponentComponent
        if (auto* ec = e->GetComponent<OpponentComponent>()) {
            ImGui::PushID("cmp_opponent");
            ImGui::BulletText("OpponentComponent"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<OpponentComponent>();
            else {
                ImGui::DragFloat(u8"移動速度##en", &ec->moveSpeed, 0.1f, 0.0f, 50.0f);
                ImGui::DragFloat(u8"止まる距離", &ec->stopRange, 0.1f, 0.0f, 20.0f);
            }
            ImGui::PopID();
        }

        // AllyComponent（AI 仲間）
        if (auto* al = e->GetComponent<AllyComponent>()) {
            ImGui::PushID("cmp_ally");
            ImGui::BulletText(u8"AllyComponent（AI 仲間）"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<AllyComponent>();
            else {
                ImGui::DragFloat(u8"移動速度", &al->moveSpeed, 0.1f, 0.0f, 50.0f);
                ImGui::DragFloat(u8"止まる距離", &al->followDistance, 0.1f, 0.5f, 30.0f);
                ImGui::DragFloat(u8"動き出す距離", &al->resumeDistance, 0.1f, 0.5f, 30.0f);
                ImGui::DragFloat(u8"経路の計算間隔", &al->repathInterval, 0.05f, 0.1f, 5.0f, u8"%.2f 秒");
                if (al->resumeDistance < al->followDistance) al->resumeDistance = al->followDistance;    // 動き出す距離は、止まる距離より短くしない

                // 攻撃（AI の撃ち方。ダメージ・装弾数・リロードはロールの武器の表＝Game/Weapon.cpp。3-5）
                ImGui::Separator();
                ImGui::TextUnformatted(u8"攻撃（ダメージ・弾数はロールの武器）");
                ImGui::DragFloat(u8"射程", &al->attackRange, 0.5f, 1.0f, 100.0f, u8"%.1f m");
                ImGui::DragFloat(u8"撃つ間隔の下限", &al->attackInterval, 0.05f, 0.0f, 10.0f, u8"%.2f 秒");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"武器の間隔とこの値の、遅いほうで撃つ");
                ImGui::DragFloat(u8"狙いのずれ", &al->aimSpread, 0.1f, 0.0f, 30.0f, u8"%.1f 度");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"武器のずれに足される、AI の狙いのずれ");
                ImGui::DragFloat(u8"撃つ高さ", &al->aimHeight, 0.05f, 0.0f, 5.0f, u8"%.2f m");
                ImGui::DragFloat(u8"敵を選び直す間隔", &al->searchInterval, 0.05f, 0.05f, 5.0f, u8"%.2f 秒");
                ImGui::DragFloat(u8"構える時間", &al->reactionTime, 0.05f, 0.0f, 5.0f, u8"%.2f 秒");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip(u8"新しい敵を選んでから、最初に撃つまでの時間");
            }
            ImGui::PopID();
        }

        // HPComponent（表示＋削除）
        if (auto* hpc = e->GetComponent<HPComponent>()) {
            ImGui::PushID("cmp_hp");
            ImGui::BulletText("HPComponent"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<HPComponent>();
            else {
                ImGui::DragFloat("HP", &hpc->hp, 1.0f, 0.0f, hpc->maxHp);
                ImGui::DragFloat(u8"最大HP", &hpc->maxHp, 1.0f, 1.0f, 10000.0f);
            }
            ImGui::PopID();
        }

        // HitboxComponent（当たり判定）
        if (auto* hb = e->GetComponent<HitboxComponent>()) {
            ImGui::PushID("cmp_hitbox");
            ImGui::BulletText(u8"Hitbox（胴体のカプセル＋頭の球）"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<HitboxComponent>();
            else {
                ImGui::DragFloat(u8"胴体の下端", &hb->bodyBottom, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat(u8"胴体の上端", &hb->bodyTop, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat(u8"胴体の半径", &hb->bodyRadius, 0.01f, 0.01f, 5.0f);
                ImGui::DragFloat(u8"頭の高さ", &hb->headHeight, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat(u8"頭の半径", &hb->headRadius, 0.01f, 0.01f, 5.0f);
                ImGui::DragFloat(u8"頭のダメージ倍率", &hb->headMultiplier, 0.05f, 1.0f, 10.0f);
            }
            ImGui::PopID();
        }

        // NavObstacleComponent（NavGrid を塞ぐ箱）
        if (auto* ob = e->GetComponent<NavObstacleComponent>()) {
            ImGui::PushID("cmp_nav");
            ImGui::BulletText("NavObstacle"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<NavObstacleComponent>();
            else { ImGui::DragFloat(u8"半分の幅 X", &ob->halfX, 0.05f, 0.05f, 50.0f); ImGui::DragFloat(u8"半分の幅 Z", &ob->halfZ, 0.05f, 0.05f, 50.0f); }
            ImGui::PopID();
        }

        // SpawnerComponent（敵をウェーブで湧かせる）
        if (auto* sp = e->GetComponent<SpawnerComponent>()) {
            ImGui::PushID("cmp_spawner");
            ImGui::BulletText(u8"Spawner（ウェーブ）"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<SpawnerComponent>();
            else {
                editStr("Prefab", sp->prefabPath);
                if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の Prefab をここへドロップ
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PREFAB"))
                        if (pl->Data) sp->prefabPath = static_cast<const char*>(pl->Data);
                    ImGui::EndDragDropTarget();
                }
                ImGui::DragInt(u8"ウェーブ数", &sp->waveCount, 0.1f, 1, 99);
                ImGui::DragInt(u8"最初のウェーブの数", &sp->baseCount, 0.1f, 1, 100);
                ImGui::DragInt(u8"ウェーブごとの増加", &sp->countIncrease, 0.1f, 0, 50);
                ImGui::DragFloat(u8"出現の間隔", &sp->spawnInterval, 0.05f, 0.05f, 10.0f);
                ImGui::DragFloat(u8"開始までの待ち", &sp->firstDelay, 0.05f, 0.0f, 30.0f);
                ImGui::DragFloat(u8"ウェーブ間の休憩", &sp->waveDelay, 0.05f, 0.0f, 30.0f);
                ImGui::DragFloat(u8"出現の半径", &sp->spawnRadius, 0.05f, 0.0f, 50.0f);
            }
            ImGui::PopID();
        }

        // GameRuleComponent（勝敗判定 → Scene 遷移）
        if (auto* gr = e->GetComponent<GameRuleComponent>()) {
            ImGui::PushID("cmp_rule");
            ImGui::BulletText(u8"GameRule（勝敗判定）"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<GameRuleComponent>();
            else {
                editStr(u8"勝ったときの Scene", gr->winScene);
                editStr(u8"負けたときの Scene", gr->loseScene);
                ImGui::DragFloat(u8"決着後の待ち時間", &gr->endDelay, 0.05f, 0.0f, 10.0f);
            }
            ImGui::PopID();
        }

        // ScreenUIComponent（タイトル・リザルト画面）
        if (auto* ui = e->GetComponent<ScreenUIComponent>()) {
            ImGui::PushID("cmp_ui");
            ImGui::BulletText(u8"ScreenUI（タイトル・リザルト画面）"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<ScreenUIComponent>();
            else {
                editStr(u8"タイトル", ui->title);
                editStr(u8"サブタイトル", ui->subtitle);
                editStr(u8"ボタンの文字", ui->buttonText);
                editStr(u8"次の Scene", ui->nextScene);
                ImGui::Checkbox(u8"結果を表示", &ui->showResult);
                ImGui::Checkbox(u8"ロールを選ぶ画面にする", &ui->roleSelect);
            }
            ImGui::PopID();
        }
        // PartySetupComponent（Stage の開始時に3人のチームをそろえる）
        if (auto* ps = e->GetComponent<PartySetupComponent>()) {
            ImGui::PushID("cmp_party");
            ImGui::BulletText(u8"PartySetup（3人のチーム）"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<PartySetupComponent>();
            else {
                editStr(u8"AI 仲間の Prefab", ps->allyPrefab);
                if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の Prefab をここへドロップ
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_PREFAB"))
                        if (pl->Data) ps->allyPrefab = static_cast<const char*>(pl->Data);
                    ImGui::EndDragDropTarget();
                }
                ImGui::DragFloat(u8"後ろへの距離", &ps->spawnBack, 0.05f, 0.0f, 20.0f);
                ImGui::DragFloat(u8"左右の間隔", &ps->spawnSide, 0.05f, 0.0f, 20.0f);
            }
            ImGui::PopID();
        }
        // GuideComponent（案内ロボット。経由点を順に進み、止まる場所でプレイヤーの合図を待つ。5-1）
        if (auto* gd = e->GetComponent<GuideComponent>()) {
            ImGui::PushID("cmp_guide");
            ImGui::BulletText(u8"Guide（案内ロボット）"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<GuideComponent>();
            else {
                ImGui::DragFloat(u8"進む速さ", &gd->moveSpeed, 0.05f, 0.1f, 20.0f, "%.2f m/s");
                ImGui::DragFloat(u8"合図できる距離", &gd->signalRange, 0.05f, 0.5f, 20.0f, "%.1f m");
                ImGui::DragFloat(u8"印の高さ", &gd->markerHeight, 0.05f, 0.0f, 10.0f, "%.2f m");
                ImGui::TextDisabled(u8"経由点（GuidePoint）を番号の順に進む");
            }
            ImGui::PopID();
        }
        // GuidePointComponent（案内ロボットの道の経由点。5-1）
        if (auto* gp = e->GetComponent<GuidePointComponent>()) {
            ImGui::PushID("cmp_guidepoint");
            ImGui::BulletText(u8"GuidePoint（道の経由点）"); ImGui::SameLine();
            if (ImGui::SmallButton(u8"削除")) e->RemoveComponent<GuidePointComponent>();
            else {
                ImGui::DragInt(u8"順番", &gp->order, 0.1f, 0, 999);
                ImGui::Checkbox(u8"ここで止まる", &gp->stopHere);
            }
            ImGui::PopID();
        }

        // + コンポーネントを追加（UEのドロップダウン）
        // ※ Inspector の一番下に置く。新しい Component の編集欄は、これより上（ScreenUI の後ろ）に足す
        ImGui::Separator();
        if (ImGui::Button(u8"+ コンポーネントを追加")) ImGui::OpenPopup("AddComponentPopup");
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (!e->GetComponent<MeshRendererComponent>() && ImGui::MenuItem(u8"MeshRendererComponent（メッシュ表示）")) e->AddComponent<MeshRendererComponent>();
            if (!e->GetComponent<SkinnedMeshComponent>() && ImGui::MenuItem(u8"SkinnedMeshComponent（アニメ付きメッシュ）")) e->AddComponent<SkinnedMeshComponent>();
            if (!e->GetComponent<AnimatorComponent>() && ImGui::MenuItem(u8"AnimatorComponent（アニメの制御）")) e->AddComponent<AnimatorComponent>();
            if (!e->GetComponent<ShaderComponent>() && ImGui::MenuItem(u8"ShaderComponent（描き方・材質）")) e->AddComponent<ShaderComponent>();
            ImGui::Separator();
            if (!e->GetComponent<PlayerComponent>() && ImGui::MenuItem(u8"PlayerComponent（操作キャラ）")) e->AddComponent<PlayerComponent>();
            if (!e->GetComponent<OpponentComponent>() && ImGui::MenuItem(u8"OpponentComponent（敵）"))  e->AddComponent<OpponentComponent>();
            if (!e->GetComponent<AllyComponent>() && ImGui::MenuItem(u8"AllyComponent（AI 仲間）")) e->AddComponent<AllyComponent>();
            if (!e->GetComponent<HPComponent>() && ImGui::MenuItem(u8"HPComponent（体力）"))      e->AddComponent<HPComponent>();
            if (!e->GetComponent<HitboxComponent>() && ImGui::MenuItem(u8"HitboxComponent（当たり判定）")) e->AddComponent<HitboxComponent>();
            if (!e->GetComponent<NavObstacleComponent>() && ImGui::MenuItem(u8"NavObstacleComponent（経路探索の障害物）")) e->AddComponent<NavObstacleComponent>();
            if (!e->GetComponent<SpawnerComponent>() && ImGui::MenuItem(u8"SpawnerComponent（敵の出現・ウェーブ）")) e->AddComponent<SpawnerComponent>();
            if (!e->GetComponent<GameRuleComponent>() && ImGui::MenuItem(u8"GameRuleComponent（勝敗判定）")) e->AddComponent<GameRuleComponent>();
            if (!e->GetComponent<ScreenUIComponent>() && ImGui::MenuItem(u8"ScreenUIComponent（タイトル・リザルト画面）")) e->AddComponent<ScreenUIComponent>();
            if (!e->GetComponent<PartySetupComponent>() && ImGui::MenuItem(u8"PartySetupComponent（3人のチーム）")) e->AddComponent<PartySetupComponent>();
            if (!e->GetComponent<GuideComponent>() && ImGui::MenuItem(u8"GuideComponent（案内ロボット）")) e->AddComponent<GuideComponent>();
            if (!e->GetComponent<GuidePointComponent>() && ImGui::MenuItem(u8"GuidePointComponent（道の経由点）")) e->AddComponent<GuidePointComponent>();
            ImGui::EndPopup();
        }
        ImGui::End();
    }

} // namespace uel
