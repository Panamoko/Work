#include "Engine/Editor/ViewportPanel.h"
#include "Engine/Systems/RenderSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/MeshShader.h"
#include "Engine/Editor/EntityCommands.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Game/PlayerComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/InputState.h"
#include "Engine/Scene/HPComponent.h"
#include <d3d11.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include <string>
#include <cstdio>
namespace uel {

    void ViewportPanel::Initialize(ID3D11Device* device)
    {
        rt.Create(device, viewWidth, viewHeight);
        shader.Initialize(device);
        cubeMesh = Mesh::CreateCube(device);
        DirectX::XMStoreFloat4x4(&view, DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&proj, DirectX::XMMatrixIdentity());
    }

    void ViewportPanel::DrawUI(ID3D11Device* device, World& world, EntityId& selected, CommandHistory& history,
        bool playMode, MeshCache& cache)
    {
        ImGuizmo::BeginFrame();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");

        if (!playMode) {
            auto modeButton = [&](const char* label, int op) {
                bool active = (gizmoOp == op);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.80f, 1.0f));
                if (ImGui::SmallButton(label)) gizmoOp = op;
                if (active) ImGui::PopStyleColor();
                };
            modeButton("Translate", ImGuizmo::TRANSLATE); ImGui::SameLine();
            modeButton("Rotate", ImGuizmo::ROTATE);    ImGui::SameLine();
            modeButton("Scale", ImGuizmo::SCALE);
            ImGui::SameLine(); ImGui::Checkbox("Grid", &showGrid);
            ImGui::SameLine(); ImGui::Checkbox("Snap", &snapEnabled);
            ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##snapT", &snapTranslate, 0.1f, 0.01f, 100.0f, "T:%.2f");
            ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##snapR", &snapRotate, 1.0f, 1.0f, 180.0f, "R:%.0f");
            ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##snapS", &snapScale, 0.05f, 0.01f, 10.0f, "S:%.2f");
        }
        else {
            ImGui::TextUnformatted("PLAY (RMB:look  WASD:move  Space:jump  LMB:shoot)");
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        int w = (int)avail.x, h = (int)avail.y;
        if (w > 0 && h > 0 && (w != viewWidth || h != viewHeight)) {
            viewWidth = w; viewHeight = h;
            rt.Resize(device, w, h);
        }

        using namespace DirectX;
        float aspect = (viewHeight > 0) ? (float)viewWidth / (float)viewHeight : 1.0f;

        // ビュー行列
        if (playMode) {
            Entity* p = world.Get(selected);
            if (p) {
                auto* pc = p->GetComponent<PlayerComponent>();
                if (!pc) pc = p->AddComponent<PlayerComponent>();

                ImGuiIO& io = ImGui::GetIO();
                bool actv = ImGui::IsWindowHovered() || ImGui::IsWindowFocused();
                InputState in;
                in.enabled = actv && !io.WantTextInput;
                in.keyW = ImGui::IsKeyDown('W'); in.keyS = ImGui::IsKeyDown('S');
                in.keyD = ImGui::IsKeyDown('D'); in.keyA = ImGui::IsKeyDown('A');
                in.jumpPressed = ImGui::IsKeyPressed(' ', false);
                in.shootClicked = ImGui::IsMouseClicked(0);
                in.lookActive = io.MouseDown[1];
                in.mouseDX = io.MouseDelta.x; in.mouseDY = io.MouseDelta.y;
                float dt = (io.DeltaTime > 0.0f) ? io.DeltaTime : (1.0f / 60.0f);

                // 全Entityの全ComponentをTick（Play中のGameUpdate）
                for (const auto& slot : world.Slots()) {
                    if (!slot) continue;
                    Entity* e = slot.get();
                    GameContext ctx{ world, *e, cache, device, dt, in };
                    for (auto& c : e->components) c->Update(ctx);
                }

                view = pc->GetView();   // プレイヤーのカメラ
            }
            else {
                view = camera.GetView();
            }
        }
        else {
            camera.Update(ImGui::IsWindowHovered());
            view = camera.GetView();
        }
        XMStoreFloat4x4(&proj, XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f));

        ImVec2 imgPos = ImGui::GetCursorScreenPos();
        if (rt.GetSRV())
            ImGui::Image((ImTextureID)rt.GetSRV(), ImVec2((float)viewWidth, (float)viewHeight));

        // クロスヘア（Play）
        if (playMode) {
            ImVec2 c(imgPos.x + viewWidth * 0.5f, imgPos.y + viewHeight * 0.5f);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), IM_COL32(255, 255, 255, 220), 2.0f);
            dl->AddLine(ImVec2(c.x, c.y - 8), ImVec2(c.x, c.y + 8), IM_COL32(255, 255, 255, 220), 2.0f);
        }

        // 編集時のみ：ドロップ・キー・グリッド・ギズモ
        if (playMode) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 c(imgPos.x + viewWidth * 0.5f, imgPos.y + viewHeight * 0.5f);

            // クロスヘア
            dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), IM_COL32(255, 255, 255, 220), 2.0f);
            dl->AddLine(ImVec2(c.x, c.y - 8), ImVec2(c.x, c.y + 8), IM_COL32(255, 255, 255, 220), 2.0f);

            Entity* p = world.Get(selected);
            PlayerComponent* pc = p ? p->GetComponent<PlayerComponent>() : nullptr;

            // ヒットマーカー（命中時に赤い×）
            if (pc && pc->GetHitMarker() > 0.0f) {
                ImU32 col = IM_COL32(255, 60, 60, 255);
                dl->AddLine(ImVec2(c.x - 12, c.y - 12), ImVec2(c.x - 4, c.y - 4), col, 2.0f);
                dl->AddLine(ImVec2(c.x + 12, c.y - 12), ImVec2(c.x + 4, c.y - 4), col, 2.0f);
                dl->AddLine(ImVec2(c.x - 12, c.y + 12), ImVec2(c.x - 4, c.y + 4), col, 2.0f);
                dl->AddLine(ImVec2(c.x + 12, c.y + 12), ImVec2(c.x + 4, c.y + 4), col, 2.0f);
            }

            // プレイヤーHPバー
            if (p) {
                if (auto* php = p->GetComponent<HPComponent>()) {
                    float bx = imgPos.x + 20.0f, by = imgPos.y + viewHeight - 40.0f, bw = 220.0f, bh = 18.0f;
                    float frac = (php->maxHp > 0.0f) ? php->hp / php->maxHp : 0.0f;
                    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(40, 40, 40, 200));
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * frac, by + bh), IM_COL32(220, 60, 60, 230));
                    dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(255, 255, 255, 180));
                    char hpTxt[32]; snprintf(hpTxt, sizeof(hpTxt), "HP %.0f/%.0f", php->hp, php->maxHp);
                    dl->AddText(ImVec2(bx + 6, by + 1), IM_COL32(255, 255, 255, 255), hpTxt);

                    if (php->hp <= 0.0f)
                        dl->AddText(ImVec2(c.x - 45, c.y - 40), IM_COL32(255, 80, 80, 255), "YOU DIED");
                }
            }
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