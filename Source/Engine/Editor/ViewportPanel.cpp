#include "Engine/Editor/ViewportPanel.h"
#include "Engine/Systems/RenderSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/MeshShader.h"
#include <d3d11.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include "Engine/Editor/EntityCommands.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Assets/MeshCache.h"
#include <string>
namespace uel {

    void ViewportPanel::Initialize(ID3D11Device* device)
    {

        rt.Create(device, viewWidth, viewHeight);
        shader.Initialize(device);
        cubeMesh = Mesh::CreateCube(device);
        DirectX::XMStoreFloat4x4(&view, DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&proj, DirectX::XMMatrixIdentity());
    }

    void ViewportPanel::DrawUI(ID3D11Device* device, World& world, EntityId& selected, CommandHistory& history) 
    {
        ImGuizmo::BeginFrame();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");

        // --- Gizmo モード切替ツールバー（キー不要で確実に切替）---
        auto modeButton = [&](const char* label, int op) {
            bool active = (gizmoOp == op);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.80f, 1.0f));
            if (ImGui::SmallButton(label)) gizmoOp = op;
            if (active) ImGui::PopStyleColor();
            };
        modeButton("Translate", ImGuizmo::TRANSLATE); ImGui::SameLine();
        modeButton("Rotate", ImGuizmo::ROTATE);    ImGui::SameLine();
        modeButton("Scale", ImGuizmo::SCALE);
        ImGuiIO& io = ImGui::GetIO();
        if ((ImGui::IsWindowFocused() || ImGui::IsWindowHovered())
            && !io.WantTextInput && !ImGui::IsMouseDown(1))  
        {
            if (ImGui::IsKeyPressed('W')) gizmoOp = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed('E')) gizmoOp = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed('R')) gizmoOp = ImGuizmo::SCALE;
        }
        ImVec2 avail = ImGui::GetContentRegionAvail();
        int w = (int)avail.x, h = (int)avail.y;
        if (w > 0 && h > 0 && (w != viewWidth || h != viewHeight)) {
            viewWidth = w; viewHeight = h;
            rt.Resize(device, w, h);
        }

        using namespace DirectX;
        camera.Update(ImGui::IsWindowHovered());   
        view = camera.GetView();

        float aspect = (viewHeight > 0) ? (float)viewWidth / (float)viewHeight : 1.0f;
        XMMATRIX p = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f);
        XMStoreFloat4x4(&proj, p);

        ImVec2 imgPos = ImGui::GetCursorScreenPos();
        if (rt.GetSRV())
            ImGui::Image((ImTextureID)rt.GetSRV(), ImVec2((float)viewWidth, (float)viewHeight));

        // ★ Asset Browser からのドロップで Entity 生成
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_GLB"))
            {
                std::string p = payload->Data ? static_cast<const char*>(payload->Data) : "";
                if (!p.empty())
                {
                    std::string name = p.substr(p.find_last_of('/') + 1);
                    EntityId id = EditorCreateEntity(world, history, name.c_str());
                    if (Entity* e = world.Get(id))
                        if (auto* m = e->GetComponent<MeshRendererComponent>())
                            m->assetId = p;
                    selected = id;
                }
            }
            ImGui::EndDragDropTarget();
        }

        Entity* e = world.Get(selected);
        TransformComponent* t = e ? e->GetComponent<TransformComponent>() : nullptr;
        if (t) {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(imgPos.x, imgPos.y, (float)viewWidth, (float)viewHeight);

            XMFLOAT4X4 model = t->GetWorldMatrix();
            ImGuizmo::Manipulate(&view._11, &proj._11,
                (ImGuizmo::OPERATION)gizmoOp, ImGuizmo::LOCAL, &model._11);

            bool usingNow = ImGuizmo::IsUsing();
            if (usingNow && !gizmoSession.active)     // 変更前を先に記録
                BeginTransformEdit(gizmoSession, selected, *t);

            if (usingNow) {
                float tr[3], rot[3], sc[3];
                ImGuizmo::DecomposeMatrixToComponents(&model._11, tr, rot, sc);
                t->position = { tr[0], tr[1], tr[2] };
                t->rotationEuler = { rot[0], rot[1], rot[2] };
                t->scale = { sc[0], sc[1], sc[2] };
            }

            if (!usingNow && gizmoSession.active)      // ドラッグ終了で1コマンド
                CommitTransformEdit(gizmoSession, world, history);
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::RenderScene(ID3D11DeviceContext* context, World& world, MeshCache& cache, ID3D11Device* device)
    {
        rt.Begin(context, 0.10f, 0.12f, 0.15f, 1.0f);
        if (rt.GetWidth() <= 0 || rt.GetHeight() <= 0 || !cubeMesh) return;

        using namespace DirectX;
        XMFLOAT4X4 viewProj;
        XMStoreFloat4x4(&viewProj, XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));

        RenderSystem::Render(world, shader, *cubeMesh, cache, device, context, viewProj);
    }

} // namespace uel