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
            char label[128];
            snprintf(label, sizeof(label), "%s##%u", e->name.c_str(), e->id.index);
            if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
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
            ImGui::Separator(); ImGui::TextUnformatted("MeshRenderer");
            char buf[260]; strncpy_s(buf, sizeof(buf), m->assetId.c_str(), _TRUNCATE);
            if (ImGui::InputText("Asset (GLB path)", buf, sizeof(buf))) m->assetId = buf;
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
            if (SkinnedMeshComponent* sk = e->GetComponent<SkinnedMeshComponent>())
            {
                ImGui::Separator(); ImGui::TextUnformatted("SkinnedMesh (Animation)");
                char sbuf[260]; strncpy_s(sbuf, sizeof(sbuf), sk->assetPath.c_str(), _TRUNCATE);
                if (ImGui::InputText("Skinned GLB", sbuf, sizeof(sbuf))) sk->assetPath = sbuf;   // ★メッシュ欄を復活
                char abuf[260]; strncpy_s(abuf, sizeof(abuf), sk->animPath.c_str(), _TRUNCATE);
                if (ImGui::InputText("Anim GLB (option)", abuf, sizeof(abuf))) sk->animPath = abuf;
                ImGui::Checkbox("Visible##sk", &sk->visible);
                ImGui::ColorEdit4("Tint##sk", &sk->color.x);

                if (!sk->assetPath.empty()) {
                    auto model = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
                    if (model && model->Valid()) {
                        const auto& clips = model->Cpu().clips;
                        ImGui::Text("Bones: %d  Clips: %d",
                            (int)model->Cpu().skeleton.bones.size(), (int)clips.size());
                        if (!clips.empty()) {
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
                    }
                    else {
                        ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "load failed (skin+mesh の .glb ?)");
                    }
                }
            }
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


        // + Add Component（UEのドロップダウン）
        if (ImGui::Button("+ Add Component")) ImGui::OpenPopup("AddComponentPopup");
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (!e->GetComponent<PlayerComponent>() && ImGui::MenuItem("PlayerComponent")) e->AddComponent<PlayerComponent>();
            if (!e->GetComponent<OpponentComponent>() && ImGui::MenuItem("OpponentComponent"))  e->AddComponent<OpponentComponent>();
            if (!e->GetComponent<HPComponent>() && ImGui::MenuItem("HPComponent"))      e->AddComponent<HPComponent>();
            if (!e->GetComponent<SkinnedMeshComponent>() && ImGui::MenuItem("SkinnedMeshComponent")) e->AddComponent<SkinnedMeshComponent>();
            if (!e->GetComponent<NavObstacleComponent>() && ImGui::MenuItem("NavObstacleComponent")) e->AddComponent<NavObstacleComponent>();
            if (!e->GetComponent<ScreenUIComponent>() && ImGui::MenuItem("ScreenUIComponent")) e->AddComponent<ScreenUIComponent>();
            if (!e->GetComponent<GameRuleComponent>() && ImGui::MenuItem("GameRuleComponent")) e->AddComponent<GameRuleComponent>();
            
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
        std::string& renameSceneRequested)
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

        if (!scanned || ImGui::Button("Rescan"))
        {
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
                        if (ext != ".glb") continue;
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
        if (ImGui::Button("Assign to Selected") && hasPick) {
            if (Entity* e = world.Get(selected)) {
                auto* m = e->GetComponent<MeshRendererComponent>();
                if (!m) m = e->AddComponent<MeshRendererComponent>();
                m->assetId = picked;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Entity") && hasPick) {
            std::string name = picked.substr(picked.find_last_of('/') + 1);
            EntityId id = EditorCreateEntity(world, history, name.c_str());
            if (Entity* e = world.Get(id))
                if (auto* m = e->GetComponent<MeshRendererComponent>())
                    m->assetId = picked;
            selected = id;
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

                    char item[320];
                    snprintf(item, sizeof(item), "%s##%s", name.c_str(), f.c_str());
                    if (ImGui::Selectable(item, f == picked))
                        picked = f;

                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("ASSET_GLB", f.c_str(), f.size() + 1);
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

    void DrawEnvironmentPanel(SceneLighting& light)
    {
        ImGui::Begin("Environment");
        ImGui::DragFloat3("Light Dir", &light.direction.x, 0.02f, -1.0f, 1.0f);
        ImGui::ColorEdit3("Light Color", &light.color.x);
        ImGui::DragFloat("Ambient", &light.ambient, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, 5.0f);
        ImGui::End();
    }

    void DrawAnimationEditorPanel(World& world, EntityId selected, ID3D11Device* device)
    {
        ImGui::Begin("Animation");
        Entity* e = world.Get(selected);
        SkinnedMeshComponent* sk = e ? e->GetComponent<SkinnedMeshComponent>() : nullptr;
        if (!sk) { ImGui::TextUnformatted("SkinnedMeshComponent を持つEntityを選択"); ImGui::End(); return; }
        if (sk->assetPath.empty()) { ImGui::TextUnformatted("Skinned GLB 未設定"); ImGui::End(); return; }

        auto model = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
        if (!model || !model->Valid()) { ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "load failed"); ImGui::End(); return; }
        const auto& clips = model->Cpu().clips;
        if (clips.empty()) { ImGui::TextUnformatted("clips なし"); ImGui::End(); return; }
        if (sk->clipIndex < 0 || sk->clipIndex >= (int)clips.size()) sk->clipIndex = 0;

        // 左：クリップ一覧
        ImGui::BeginChild("clips", ImVec2(170, 0), true);
        ImGui::TextUnformatted("Clips"); ImGui::Separator();
        for (int i = 0; i < (int)clips.size(); ++i) {
            std::string nm = clips[i].name.empty() ? ("clip " + std::to_string(i)) : clips[i].name;
            char buf[96]; snprintf(buf, sizeof(buf), "%s  (%.2fs)", nm.c_str(), clips[i].duration);
            if (ImGui::Selectable(buf, i == sk->clipIndex)) { sk->clipIndex = i; sk->time = 0.0f; }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("timeline", ImVec2(0, 0), false);

        const AnimationClip& clip = clips[sk->clipIndex];
        float dur = clip.duration > 0.0f ? clip.duration : 1.0f;

        // トランスポート
        if (ImGui::Button(sk->playing ? "Pause" : "Play")) sk->playing = !sk->playing;
        ImGui::SameLine();
        if (ImGui::Button("Stop")) { sk->playing = false; sk->time = 0.0f; }
        ImGui::SameLine(); ImGui::Checkbox("Loop", &sk->loop);
        ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::DragFloat("Speed", &sk->speed, 0.01f, -4.0f, 4.0f);
        ImGui::Text("Time  %.3f / %.3f s    Bones %d",
            sk->time, dur, (int)model->Cpu().skeleton.bones.size());

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
            sk->time = t; sk->playing = false;
        }
        // プレイヘッド
        float px = p0.x + (sk->time / dur) * w;
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