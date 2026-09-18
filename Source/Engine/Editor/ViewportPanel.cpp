#include "Engine/Editor/ViewportPanel.h"
#include "Engine/Systems/RenderSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include <d3d11.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>

namespace uel {

    void ViewportPanel::Initialize(ID3D11Device* device)
    {
        rt.Create(device, viewWidth, viewHeight);
        cube.Initialize(device);
        DirectX::XMStoreFloat4x4(&view, DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&proj, DirectX::XMMatrixIdentity());
    }

    void ViewportPanel::DrawUI(ID3D11Device* device, World& world, EntityId selected, CommandHistory& history)
    {
        ImGuizmo::BeginFrame();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");

        ImVec2 avail = ImGui::GetContentRegionAvail();
        int w = (int)avail.x, h = (int)avail.y;
        if (w > 0 && h > 0 && (w != viewWidth || h != viewHeight)) {
            viewWidth = w; viewHeight = h;
            rt.Resize(device, w, h);
        }

        using namespace DirectX;
        float aspect = (viewHeight > 0) ? (float)viewWidth / (float)viewHeight : 1.0f;
        XMMATRIX v = XMMatrixLookAtLH(
            XMVectorSet(4.0f, 4.0f, -7.0f, 0.0f),
            XMVectorSet(0.0f, 0.5f, 0.0f, 0.0f),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        XMMATRIX p = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f);
        XMStoreFloat4x4(&view, v);
        XMStoreFloat4x4(&proj, p);

        ImVec2 imgPos = ImGui::GetCursorScreenPos();
        if (rt.GetSRV())
            ImGui::Image((ImTextureID)rt.GetSRV(), ImVec2((float)viewWidth, (float)viewHeight));

        ImGuiIO& io = ImGui::GetIO();
        if ((ImGui::IsWindowFocused() || ImGui::IsWindowHovered()) && !io.WantTextInput) {
            if (ImGui::IsKeyPressed('W')) gizmoOp = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed('E')) gizmoOp = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed('R')) gizmoOp = ImGuizmo::SCALE;
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

    void ViewportPanel::RenderScene(ID3D11DeviceContext* context, World& world)
    {
        rt.Begin(context, 0.10f, 0.12f, 0.15f, 1.0f);
        if (rt.GetWidth() <= 0 || rt.GetHeight() <= 0) return;

        using namespace DirectX;
        XMFLOAT4X4 viewProj;
        XMStoreFloat4x4(&viewProj, XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));
        RenderSystem::Render(world, cube, context, viewProj);
    }

} // namespace uel