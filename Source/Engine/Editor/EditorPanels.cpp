#include "Engine/Editor/EditorPanels.h"
#include "Engine/Editor/CommandHistory.h"
#include "Engine/Editor/EntityCommands.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/PrefabInstanceComponent.h"
#include "Engine/Serialization/StageSerializer.h"
#include "Engine/Game/PlayerComponent.h"
#include "Engine/Game/OpponentComponent.h"
#include "Engine/Graphics/SceneLighting.h"
#include "Engine/Graphics/PostProcessSettings.h"
#include "Engine/AI/EngineNavGrid.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// エディタのパネルのうち、Hierarchy・ショートカット・メニューバー・Environment・NavGrid。
// Inspector は InspectorPanel.cpp、Asset Browser は AssetBrowserPanel.cpp、アニメ関連の UI は AnimEditorPanels.cpp にある。
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
        float px = (std::min)(avail.x / cols, avail.y / rows); if (px < 2.0f) px = 2.0f;
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
