#include "Engine/Editor/EditorPanels.h"
#include "Engine/Editor/EntityCommands.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Config/EngineConfig.h"

#include <imgui.h>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

// Asset Browser パネル（Data/ 以下の GLB・Prefab・アニメグラフの一覧と配置）。
namespace uel {

    namespace {
        bool        g_assetRescan = false;   // Prefab保存後に Asset Browser を再スキャンさせる

        bool IsPrefabPath(const std::string& p) {
            const std::string suf = ".prefab.json";
            return p.size() >= suf.size() && p.compare(p.size() - suf.size(), suf.size(), suf) == 0;
        }
        bool IsAnimGraphPath(const std::string& p) {
            const std::string suf = ".animgraph.json";
            return p.size() >= suf.size() && p.compare(p.size() - suf.size(), suf.size(), suf) == 0;
        }
    }

    void RequestAssetRescan()
    {
        g_assetRescan = true;
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

} // namespace uel
