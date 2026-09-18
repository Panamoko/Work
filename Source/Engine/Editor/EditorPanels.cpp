#include "Engine/Editor/EditorPanels.h"
#include "Engine/Editor/TransformEdit.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Serialization/StageSerializer.h"   
#include <imgui.h>
#include <cstdio>

namespace uel {

    void DrawHierarchyPanel(World& world, EntityId& selected)
    {
        ImGui::Begin("Hierarchy");
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

    void DrawInspectorPanel(World& world, EntityId selected, CommandHistory& history, TransformEditSession& session)
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
            if (ImGui::IsItemActivated())              BeginTransformEdit(session, selected, *t);
            if (ImGui::IsItemDeactivatedAfterEdit())   CommitTransformEdit(session, world, history);

            ImGui::DragFloat3("Rotation", &t->rotationEuler.x, 0.5f);
            if (ImGui::IsItemActivated())              BeginTransformEdit(session, selected, *t);
            if (ImGui::IsItemDeactivatedAfterEdit())   CommitTransformEdit(session, world, history);

            ImGui::DragFloat3("Scale", &t->scale.x, 0.05f, 0.001f, 1000.0f);
            if (ImGui::IsItemActivated())              BeginTransformEdit(session, selected, *t);
            if (ImGui::IsItemDeactivatedAfterEdit())   CommitTransformEdit(session, world, history);
        }

        if (MeshRendererComponent* m = e->GetComponent<MeshRendererComponent>())
        {
            ImGui::Separator(); ImGui::TextUnformatted("MeshRenderer");
            ImGui::Checkbox("Visible", &m->visible);   // ※Undo未対応（後で）
            ImGui::ColorEdit4("Color", &m->color.x);   // ※Undo未対応（後で）
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

} // namespace uel