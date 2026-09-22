#include "Engine/Editor/EditorCamera.h"
#include <imgui.h>
#include <cmath>

using namespace DirectX;

namespace uel {

    XMFLOAT3 EditorCamera::ForwardVector() const
    {
        float cp = cosf(pitch), sp = sinf(pitch);
        float sy = sinf(yaw), cy = cosf(yaw);
        return { cp * sy, sp, cp * cy };
    }

    void EditorCamera::Update(bool viewportHovered)
    {
        ImGuiIO& io = ImGui::GetIO();

        // 右クリックで操作開始（Viewportホバー時）。離すまで継続。
        if (io.MouseDown[1]) {
            if (!controlling && viewportHovered) controlling = true;
        }
        else {
            controlling = false;
        }
        if (!controlling) return;

        // 回転
        yaw += io.MouseDelta.x * lookSens;
        pitch -= io.MouseDelta.y * lookSens;
        const float lim = 1.55f;
        if (pitch > lim) pitch = lim;
        if (pitch < -lim) pitch = -lim;

        // 移動
        float dt = (io.DeltaTime > 0.0f) ? io.DeltaTime : (1.0f / 60.0f);
        float speed = moveSpeed * dt;

        XMFLOAT3 fwd = ForwardVector();
        XMVECTOR F = XMLoadFloat3(&fwd);
        XMVECTOR Up = XMVectorSet(0, 1, 0, 0);
        XMVECTOR R = XMVector3Normalize(XMVector3Cross(Up, F)); // 右方向
        XMVECTOR P = XMLoadFloat3(&position);

        if (ImGui::IsKeyDown('W')) P = XMVectorAdd(P, XMVectorScale(F, speed));
        if (ImGui::IsKeyDown('S')) P = XMVectorSubtract(P, XMVectorScale(F, speed));
        if (ImGui::IsKeyDown('D')) P = XMVectorAdd(P, XMVectorScale(R, speed));
        if (ImGui::IsKeyDown('A')) P = XMVectorSubtract(P, XMVectorScale(R, speed));
        if (ImGui::IsKeyDown('E')) P = XMVectorAdd(P, XMVectorScale(Up, speed));
        if (ImGui::IsKeyDown('Q')) P = XMVectorSubtract(P, XMVectorScale(Up, speed));

        if (io.MouseWheel != 0.0f)
            P = XMVectorAdd(P, XMVectorScale(F, io.MouseWheel * 0.6f));

        XMStoreFloat3(&position, P);
    }

    XMFLOAT4X4 EditorCamera::GetView() const
    {
        XMFLOAT3 fwd = ForwardVector();
        XMVECTOR P = XMLoadFloat3(&position);
        XMVECTOR F = XMLoadFloat3(&fwd);
        XMMATRIX v = XMMatrixLookAtLH(P, XMVectorAdd(P, F), XMVectorSet(0, 1, 0, 0));
        XMFLOAT4X4 out;
        XMStoreFloat4x4(&out, v);
        return out;
    }

} // namespace uel