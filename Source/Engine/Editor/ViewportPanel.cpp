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
#include "Engine/Scene/SkinnedMeshComponent.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Game/OpponentComponent.h"

#include <cmath>
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
        cubeMesh = Mesh::CreateCube(device);       
        skybox.Initialize(device);
        shadowMap.Initialize(device, 2048);   
        skinnedShader.Initialize(device);  
    }

    void ViewportPanel::DrawUI(ID3D11Device* device, World& world, EntityId& selected, CommandHistory& history,
        bool playMode, MeshCache& cache)
    {
        ImGuizmo::BeginFrame();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");
        // スキニング：クリップ時間を進める（編集/プレイ両方でプレビュー）
        {
            float dt = ImGui::GetIO().DeltaTime;
            for (const auto& slot : world.Slots()) {
                if (!slot) continue;
                auto* sk = slot->GetComponent<SkinnedMeshComponent>();
                if (!sk || !sk->playing || sk->assetPath.empty()) continue;
                auto model = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
                if (!model) continue;
                const auto& clips = model->Cpu().clips;
                if (sk->clipIndex < 0 || sk->clipIndex >= (int)clips.size()) continue;
                float dur = clips[sk->clipIndex].duration;
                sk->time += dt * sk->speed;
                if (dur > 0.0f) {
                    if (sk->loop) { sk->time = fmodf(sk->time, dur); if (sk->time < 0.0f) sk->time += dur; }
                    else if (sk->time > dur) sk->time = dur;
                }
            }
        }
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
            // プレイヤー = PlayerComponent を持つEntity
            Entity* player = nullptr;
            // NavGridをbake（Play中の経路探索）
            nav.Configure({ 0.0f, 0.0f, 0.0f }, 1.0f, 60, 60);
            nav.Bake(world);

            for (const auto& slot : world.Slots())
                if (slot && slot->GetComponent<PlayerComponent>()) { player = slot.get(); break; }

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

            for (const auto& slot : world.Slots()) {
                if (!slot) continue;
                Entity* e = slot.get();
                // NavGridをbake（Play中の経路探索）
                GameContext ctx{ world, *e, cache, device, dt, in, &nav };
                for (auto& c : e->components) c->Update(ctx);
            }

            if (player) { view = player->GetComponent<PlayerComponent>()->GetView(); }
            else { view = camera.GetView(); }
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
        // Play時オーバーレイ（クロスヘア＋ヒットマーカー＋HPバー）
        if (playMode) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 c(imgPos.x + viewWidth * 0.5f, imgPos.y + viewHeight * 0.5f);
            dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), IM_COL32(255, 255, 255, 220), 2.0f);
            dl->AddLine(ImVec2(c.x, c.y - 8), ImVec2(c.x, c.y + 8), IM_COL32(255, 255, 255, 220), 2.0f);

            Entity* p = nullptr;                                  // ★プレイヤーを探す
            for (const auto& slot : world.Slots())
                if (slot && slot->GetComponent<PlayerComponent>()) { p = slot.get(); break; }
            PlayerComponent* pc = p ? p->GetComponent<PlayerComponent>() : nullptr;

            if (pc && pc->GetHitMarker() > 0.0f) {
                ImU32 col = IM_COL32(255, 60, 60, 255);
                dl->AddLine(ImVec2(c.x - 12, c.y - 12), ImVec2(c.x - 4, c.y - 4), col, 2.0f);
                dl->AddLine(ImVec2(c.x + 12, c.y - 12), ImVec2(c.x + 4, c.y - 4), col, 2.0f);
                dl->AddLine(ImVec2(c.x - 12, c.y + 12), ImVec2(c.x - 4, c.y + 4), col, 2.0f);
                dl->AddLine(ImVec2(c.x + 12, c.y + 12), ImVec2(c.x + 4, c.y + 4), col, 2.0f);
            }
            if (p) if (auto* php = p->GetComponent<HPComponent>()) {
                float bx = imgPos.x + 20.0f, by = imgPos.y + viewHeight - 40.0f, bw = 220.0f, bh = 18.0f;
                float frac = (php->maxHp > 0.0f) ? php->hp / php->maxHp : 0.0f;
                if (frac < 0) frac = 0; if (frac > 1) frac = 1;
                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(40, 40, 40, 200));
                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * frac, by + bh), IM_COL32(220, 60, 60, 230));
                dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(255, 255, 255, 180));
                char hpTxt[32]; snprintf(hpTxt, sizeof(hpTxt), "HP %.0f/%.0f", php->hp, php->maxHp);
                dl->AddText(ImVec2(bx + 6, by + 1), IM_COL32(255, 255, 255, 255), hpTxt);
                if (php->hp <= 0.0f) dl->AddText(ImVec2(c.x - 45, c.y - 40), IM_COL32(255, 80, 80, 255), "YOU DIED");
            }
            // 敵の追跡経路を3D投影で表示
            {
                XMMATRIX vp = XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj);
                auto project = [&](XMFLOAT3 wpos, ImVec2& out) -> bool {
                    XMVECTOR clip = XMVector4Transform(XMVectorSet(wpos.x, wpos.y, wpos.z, 1.0f), vp);
                    float ww = XMVectorGetW(clip);
                    if (ww <= 0.0001f) return false;
                    float ndcX = XMVectorGetX(clip) / ww;
                    float ndcY = XMVectorGetY(clip) / ww;
                    out.x = imgPos.x + (ndcX * 0.5f + 0.5f) * viewWidth;
                    out.y = imgPos.y + (1.0f - (ndcY * 0.5f + 0.5f)) * viewHeight;
                    return true;
                    };
                for (const auto& slot : world.Slots()) {
                    if (!slot) continue;
                    auto* oc = slot->GetComponent<OpponentComponent>();
                    if (!oc) continue;
                    const auto& pth = oc->GetPath();
                    ImVec2 prev; bool havePrev = false;
                    for (const auto& wp : pth) {
                        XMFLOAT3 up = wp; up.y += 0.1f;
                        ImVec2 s; if (!project(up, s)) { havePrev = false; continue; }
                        dl->AddCircleFilled(s, 3.0f, IM_COL32(255, 220, 60, 220));
                        if (havePrev) dl->AddLine(prev, s, IM_COL32(255, 220, 60, 230), 2.0f);
                        prev = s; havePrev = true;
                    }
                }
            }
        }

        // 編集時のみ：ドロップ・キー・グリッド・ギズモ（★復元）
        if (!playMode) {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_GLB")) {
                    std::string pth = payload->Data ? static_cast<const char*>(payload->Data) : "";
                    if (!pth.empty()) {
                        std::string name = pth.substr(pth.find_last_of('/') + 1);
                        EntityId id = EditorCreateEntity(world, history, name.c_str());
                        if (Entity* e = world.Get(id))
                            if (auto* m = e->GetComponent<MeshRendererComponent>()) m->assetId = pth;
                        selected = id;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGuiIO& io = ImGui::GetIO();
            if ((ImGui::IsWindowFocused() || ImGui::IsWindowHovered()) && !io.WantTextInput && !ImGui::IsMouseDown(1)) {
                if (ImGui::IsKeyPressed('W')) gizmoOp = ImGuizmo::TRANSLATE;
                if (ImGui::IsKeyPressed('E')) gizmoOp = ImGuizmo::ROTATE;
                if (ImGui::IsKeyPressed('R')) gizmoOp = ImGuizmo::SCALE;
            }

            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(imgPos.x, imgPos.y, (float)viewWidth, (float)viewHeight);

            if (showGrid) {
                XMFLOAT4X4 gridM; XMStoreFloat4x4(&gridM, XMMatrixIdentity());
                ImGuizmo::DrawGrid(&view._11, &proj._11, &gridM._11, 10.0f);
            }

            Entity* e = world.Get(selected);
            TransformComponent* t = e ? e->GetComponent<TransformComponent>() : nullptr;
            if (t) {
                XMFLOAT4X4 model = t->GetWorldMatrix();
                float snapArr[3];
                if (gizmoOp == ImGuizmo::TRANSLATE) { snapArr[0] = snapArr[1] = snapArr[2] = snapTranslate; }
                else if (gizmoOp == ImGuizmo::ROTATE) { snapArr[0] = snapRotate; }
                else { snapArr[0] = snapScale; }
                float* snapPtr = snapEnabled ? snapArr : nullptr;

                ImGuizmo::Manipulate(&view._11, &proj._11, (ImGuizmo::OPERATION)gizmoOp, ImGuizmo::LOCAL, &model._11, nullptr, snapPtr);

                bool usingNow = ImGuizmo::IsUsing();
                if (usingNow && !gizmoSession.active) BeginTransformEdit(gizmoSession, selected, *t);
                if (usingNow) {
                    float tr[3], rot[3], sc[3];
                    ImGuizmo::DecomposeMatrixToComponents(&model._11, tr, rot, sc);
                    t->position = { tr[0],tr[1],tr[2] };
                    t->rotationEuler = { rot[0],rot[1],rot[2] };
                    t->scale = { sc[0],sc[1],sc[2] };
                }
                if (!usingNow && gizmoSession.active) CommitTransformEdit(gizmoSession, world, history);
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::RenderScene(ID3D11DeviceContext* context, World& world, MeshCache& cache,
        ID3D11Device* device, const SceneLighting& lighting)
    {
        using namespace DirectX;

        // ライトのview-proj（正射影・原点周りをカバー）
        XMVECTOR ld = XMVector3Normalize(XMVectorSet(lighting.direction.x, lighting.direction.y, lighting.direction.z, 0));
        XMVECTOR up = (fabsf(XMVectorGetY(ld)) > 0.99f) ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);
        XMVECTOR lpos = XMVectorScale(ld, -25.0f);
        XMMATRIX lView = XMMatrixLookAtLH(lpos, XMVectorSet(0, 0, 0, 0), up);
        XMMATRIX lProj = XMMatrixOrthographicLH(30.0f, 30.0f, 0.1f, 60.0f);
        XMFLOAT4X4 lightVP; XMStoreFloat4x4(&lightVP, lView * lProj);

        // シャドウパス
        shadowMap.BeginPass(context, lightVP);
        RenderSystem::RenderShadow(world, shadowMap, *cubeMesh, cache, device, context);

        // 本描画
        rt.Begin(context, 0.10f, 0.12f, 0.15f, 1.0f);
        if (rt.GetWidth() <= 0 || rt.GetHeight() <= 0 || !cubeMesh) return;

        XMFLOAT4X4 viewProj;
        XMStoreFloat4x4(&viewProj, XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));
        XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));
        XMFLOAT3 camPos; XMStoreFloat3(&camPos, invView.r[3]);

        skybox.Render(context, viewProj, camPos);
        shader.SetLight(lighting);
        shader.SetShadow(shadowMap.GetSRV(), lightVP);
        RenderSystem::Render(world, shader, *cubeMesh, cache, device, context, viewProj);
        skinnedShader.SetLight(lighting);
        RenderSystem::RenderSkinned(world, skinnedShader, GlobalSkinnedCache(), device, context, viewProj);
        // シャドウSRVを外す（次フレームのシャドウ書き込みと競合させない）
        ID3D11ShaderResourceView* nul = nullptr;
        context->PSSetShaderResources(1, 1, &nul);
    }

} // namespace uel