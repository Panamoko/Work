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

        for (const auto& slot : world.Slots())
        {
            if (!slot) continue;
            Entity* e = slot.get();
            bool isSelected = (e->id == selected);
            char label[128];
            snprintf(label, sizeof(label), "%s##%u", e->name.c_str(), e->id.index);
            if (ImGui::Selectable(label, isSelected))
                selected = e->id;
        }
        ImGui::End();
    }

    void DrawInspectorPanel(World& world, EntityId selected, CommandHistory& history,
        TransformEditSession& session, MeshCache& cache, ID3D11Device* device)
    {
        ImGui::Begin("Inspector");
        Entity* e = world.Get(selected);
        if (!e) { ImGui::TextUnformatted("(No selection)"); ImGui::End(); return; }

        ImGui::Text("Name : %s", e->name.c_str());
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
    void DrawMainMenuBar(World& world, EntityId& selected, CommandHistory& history)
    {
        const char* kPath = "Stage.json";
        bool doSave = false, doLoad = false;

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Stage", "Ctrl+S")) doSave = true;
                if (ImGui::MenuItem("Load Stage", "Ctrl+L")) doLoad = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, history.CanUndo())) history.Undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, history.CanRedo())) history.Redo();
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && io.KeyCtrl)
        {
            if (ImGui::IsKeyPressed('S')) doSave = true;
            if (ImGui::IsKeyPressed('L')) doLoad = true;
        }

        if (doSave) StageSerializer::Save(world, kPath);
        if (doLoad && StageSerializer::Load(world, kPath))
        {
            selected = EntityId{}; // 旧IDは無効化されるので選択リセット
            history.Clear();       // ロード後は履歴クリア
        }
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

} // namespace uel