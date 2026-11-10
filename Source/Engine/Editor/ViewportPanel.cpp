#include "Engine/Editor/ViewportPanel.h"
#include "Engine/Systems/RenderSystem.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/MeshShader.h"
#include "Engine/Editor/EntityCommands.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Components/PlayerComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/InputState.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Components/SkinnedMeshComponent.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Components/OpponentComponent.h"
#include "Engine/Components/ScreenUIComponent.h"
#include "Engine/Components/HitboxComponent.h"
#include "Engine/Components/SpawnerComponent.h"
#include "Engine/Config/EngineConfig.h"
#include "Engine/Components/AllyComponent.h"

#include <cfloat>
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
        rt.Create(device, viewWidth, viewHeight, true);        //  V [   p F F { @   { [ x
        finalRT.Create(device, viewWidth, viewHeight, false);  //  F    
        postProcess.Initialize(device);
        shader.Initialize(device);
        cubeMesh = Mesh::CreateCube(device);
        DirectX::XMStoreFloat4x4(&view, DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&proj, DirectX::XMMatrixIdentity());
        cubeMesh = Mesh::CreateCube(device);
        skybox.Initialize(device);
        envMap.Initialize(device);   // Data/Sprite/IBL/skybox.dds
        shadowMap.Initialize(device, 2048);
        skinnedShader.Initialize(device);

    }


    bool ViewportPanel::ReloadShaders(ID3D11Device* device)
    {
        //  V        Ă݂āA ł     ̂       ւ   i   Ȃ       O ̃V F [ _ ̂܂܁j B
        //    C g E e E i F E | X g v   Z X ̐ݒ ́A   t   [   RenderScene  œn       ̂ŁA  蒼   Ă     Ȃ  B
        bool allOk = true;
        { EngineSkybox      s; if (s.Initialize(device)) skybox = std::move(s);        else allOk = false; }
        { PostProcess       s; if (s.Initialize(device)) postProcess = std::move(s);   else allOk = false; }
        { MeshShader        s; if (s.Initialize(device)) shader = std::move(s);        else allOk = false; }
        { SkinnedMeshShader s; if (s.Initialize(device)) skinnedShader = std::move(s); else allOk = false; }
        { EngineShadowMap   s; if (s.Initialize(device, shadowMap.GetSize())) shadowMap = std::move(s); else allOk = false; }
        return allOk;
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
            modeButton(u8" ړ ", ImGuizmo::TRANSLATE); ImGui::SameLine();
            modeButton(u8"  ]", ImGuizmo::ROTATE);    ImGui::SameLine();
            modeButton(u8" g  k  ", ImGuizmo::SCALE);
            ImGui::SameLine(); ImGui::Checkbox(u8" O   b h", &showGrid);
            ImGui::SameLine(); ImGui::Checkbox(u8" X i b v", &snapEnabled);
            ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##snapT", &snapTranslate, 0.1f, 0.01f, 100.0f, "T:%.2f");
            ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##snapR", &snapRotate, 1.0f, 1.0f, 180.0f, "R:%.0f");
            ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##snapS", &snapScale, 0.05f, 0.01f, 10.0f, "S:%.2f");
            ImGui::SameLine(); ImGui::Checkbox(u8"     蔻  ", &showHitboxes);
        }
        else {
            ImGui::TextUnformatted(u8" v   C   i E h   b O F   _ @WASD F ړ  @Space F W     v @   N   b N F ˌ  j");
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        int w = (int)avail.x, h = (int)avail.y;
        if (w > 0 && h > 0 && (w != viewWidth || h != viewHeight)) {
            viewWidth = w; viewHeight = h;
            rt.Resize(device, w, h);
            finalRT.Resize(device, w, h);
        }

        using namespace DirectX;
        float aspect = (viewHeight > 0) ? (float)viewWidth / (float)viewHeight : 1.0f;

        //  r   [ s  
        if (playMode) {
            //  v   C   [ = PlayerComponent      Entity
            Entity* player = nullptr;
            // NavGrid  bake iPlay   ̌o H T   j
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
            float dt = ClampDeltaTime(io.DeltaTime);     //     t   iEngineConfig.h j

            //  Y   Ń  [ v iSpawner   X V    Entity 𑝂₵ Ă   Ȃ  悤 Ɂj
            for (size_t i = 0; i < world.Slots().size(); ++i) {
                Entity* e = world.Slots()[i].get();
                if (!e) continue;
                GameContext ctx{ world, *e, cache, device, dt, in, &nav, &flow };
                for (size_t ci = 0; ci < e->components.size(); ++ci) e->components[ci]->Update(ctx);
            }
            world.FlushDestroy();     // Update  ̒  ŗ\ 񂳂ꂽ u  ŏ    v  A     ł܂Ƃ߂ď   
            if (player) { view = player->GetComponent<PlayerComponent>()->GetView(); }
            else { view = camera.GetView(); }
        }
        else {
            camera.Update(ImGui::IsWindowHovered());
            view = camera.GetView();
        }
        XMStoreFloat4x4(&proj, XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f));

        ImVec2 imgPos = ImGui::GetCursorScreenPos();
        if (finalRT.GetSRV())
            ImGui::Image((ImTextureID)finalRT.GetSRV(), ImVec2((float)viewWidth, (float)viewHeight));
        //  q b g { b N X ̕\   i ҏW   ͑I 𒆂̂݁^ uHitbox v ` F b N őS   BPlay    L   j
        {
            XMMATRIX vpM = XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj);
            ImDrawList* hdl = ImGui::GetWindowDrawList();
            auto proj3 = [&](XMFLOAT3 p, ImVec2& out) -> bool {
                XMVECTOR c = XMVector4Transform(XMVectorSet(p.x, p.y, p.z, 1.0f), vpM);
                float ww = XMVectorGetW(c);
                if (ww <= 0.0001f) return false;
                out.x = imgPos.x + (XMVectorGetX(c) / ww * 0.5f + 0.5f) * viewWidth;
                out.y = imgPos.y + (1.0f - (XMVectorGetY(c) / ww * 0.5f + 0.5f)) * viewHeight;
                return true;
                };
            //  ~ iplane 0:    XZ / 1:XY / 2:ZY j
            auto circle = [&](XMFLOAT3 c, float r, int plane, ImU32 col) {
                const int N = 24; ImVec2 prev; bool havePrev = false;
                for (int i = 0; i <= N; ++i) {
                    float a = 6.2831853f * i / N, ca = cosf(a) * r, sa = sinf(a) * r;
                    XMFLOAT3 p = c;
                    if (plane == 0) { p.x += ca; p.z += sa; }
                    else if (plane == 1) { p.x += ca; p.y += sa; }
                    else { p.z += ca; p.y += sa; }
                    ImVec2 s; if (!proj3(p, s)) { havePrev = false; continue; }
                    if (havePrev) hdl->AddLine(prev, s, col, 1.5f);
                    prev = s; havePrev = true;
                }

                };
            auto line3 = [&](XMFLOAT3 a, XMFLOAT3 b, ImU32 col) {
                ImVec2 sa, sb; if (proj3(a, sa) && proj3(b, sb)) hdl->AddLine(sa, sb, col, 1.5f);
                };

            for (const auto& slot : world.Slots()) {
                if (!slot) continue;
                Entity* e = slot.get();
                bool isSel = (!playMode && e->id == selected);
                if (!showHitboxes && !isSel) continue;
                auto* t = e->GetComponent<TransformComponent>();
                auto* hb = e->GetComponent<HitboxComponent>();
                if (!t || !hb) continue;

                auto ws = hb->GetWorldShapes(*t);
                ImU32 bodyCol = IM_COL32(80, 220, 255, 220), headCol = IM_COL32(255, 90, 90, 230);
                for (int pl = 0; pl < 3; ++pl) {
                    circle(ws.bodyA, ws.bodyR, pl, bodyCol);
                    circle(ws.bodyB, ws.bodyR, pl, bodyCol);
                    circle(ws.headC, ws.headR, pl, headCol);
                }
                const float ox[4] = { 1, -1, 0, 0 }, oz[4] = { 0, 0, 1, -1 };
                for (int k = 0; k < 4; ++k)
                    line3({ ws.bodyA.x + ox[k] * ws.bodyR, ws.bodyA.y, ws.bodyA.z + oz[k] * ws.bodyR },
                        { ws.bodyB.x + ox[k] * ws.bodyR, ws.bodyB.y, ws.bodyB.z + oz[k] * ws.bodyR }, bodyCol);
                // Spawner ̗N   ͈́i ҏW   ɑI    Ă  鎞     A   F ̉~

            }
            if (!playMode) {
                if (Entity* se = world.Get(selected)) {
                    auto* t = se->GetComponent<TransformComponent>();
                    auto* sp = se->GetComponent<SpawnerComponent>();
                    if (t && sp) circle(t->position, sp->spawnRadius, 0, IM_COL32(255, 210, 60, 230));
                }
            }
        }
        // Play   ̃I [ o [   C i Ə  E q b g } [ J [ EHP E G o H ETitle/Result  ʁj
        if (playMode) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 c(imgPos.x + viewWidth * 0.5f, imgPos.y + viewHeight * 0.5f);

            Entity* p = nullptr;
            for (const auto& slot : world.Slots())
                if (slot && slot->GetComponent<PlayerComponent>()) { p = slot.get(); break; }
            PlayerComponent* pc = p ? p->GetComponent<PlayerComponent>() : nullptr;

            if (pc) {   //  Ə  ̓v   C   [     鎞    
                dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), IM_COL32(255, 255, 255, 220), 2.0f);
                dl->AddLine(ImVec2(c.x, c.y - 8), ImVec2(c.x, c.y + 8), IM_COL32(255, 255, 255, 220), 2.0f);
            }
            //    邢  ⏰ ̏ ł ǂ߂ 悤 ɁA     ̉E   ɍ    e  t   ĕ`  
            auto shadowText = [&](ImVec2 pos, ImU32 col, const char* txt) {
                dl->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 200), txt);
                dl->AddText(pos, col, txt);
                };
            if (pc) {   //  I 񂾃  [   i    BGameFlow  ɓ    Ă   l    ̂܂܏o   j
                char roleTxt[32]; snprintf(roleTxt, sizeof(roleTxt), "ROLE: %s", RoleName(flow.playerRole));
                shadowText(ImVec2(imgPos.x + 20.0f, imgPos.y + 12.0f), IM_COL32(255, 255, 255, 240), roleTxt);
            }
            if (pc && pc->GetHitMarker() > 0.0f) {
                ImU32 col = pc->GetHeadshot() ? IM_COL32(255, 60, 60, 255) : IM_COL32(255, 255, 255, 255);   //   = ԁ^    =  
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
            // AI    Ԃ  HP i v   C   [   HP  o [ ̏ ɁA1 l           ׂ j
            {
                const float bx = imgPos.x + 20.0f, bw = 160.0f, bh = 12.0f;
                float by = imgPos.y + viewHeight - 62.0f;
                for (const auto& slot : world.Slots()) {
                    if (!slot || !slot->GetComponent<AllyComponent>()) continue;
                    auto* ahp = slot->GetComponent<HPComponent>();
                    if (!ahp) continue;
                    float frac = (ahp->maxHp > 0.0f) ? ahp->hp / ahp->maxHp : 0.0f;
                    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(40, 40, 40, 200));
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * frac, by + bh), IM_COL32(80, 200, 120, 230));
                    dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(255, 255, 255, 160));
                    char aTxt[96]; snprintf(aTxt, sizeof(aTxt), "%s  %.0f", slot->name.c_str(), ahp->hp);
                    shadowText(ImVec2(bx + bw + 8.0f, by - 3.0f), IM_COL32(255, 255, 255, 240), aTxt);
                    by -= 20.0f;
                }
            }
            //  E F [ u \   iSpawner     鎞 j
            {
                SpawnerComponent* sp = nullptr;
                int enemies = 0;
                for (const auto& slot : world.Slots()) {
                    if (!slot) continue;
                    if (!sp) sp = slot->GetComponent<SpawnerComponent>();
                    if (slot->GetComponent<OpponentComponent>()) ++enemies;
                }
                if (sp) {
                    char wbuf[96];
                    if (sp->HasError()) snprintf(wbuf, sizeof(wbuf), "Spawner: prefab not found");
                    else snprintf(wbuf, sizeof(wbuf), "WAVE %d / %d    ENEMIES %d", sp->CurrentWave(), sp->waveCount, enemies);
                    ImVec2 ts = ImGui::CalcTextSize(wbuf);
                    dl->AddText(ImVec2(imgPos.x + (viewWidth - ts.x) * 0.5f, imgPos.y + 12.0f), IM_COL32(255, 255, 255, 230), wbuf);

                    if (sp->BannerTime() > 0.0f) {                     //  E F [ u J n ̑傫 ȕ\  
                                                char bbuf[32]; snprintf(bbuf, sizeof(bbuf), "WAVE %d", sp->CurrentWave());
                        ImFont* font = ImGui::GetFont();
                        float a = (sp->BannerTime() > 1.0f) ? 1.0f : sp->BannerTime();   //  Ō  1 b Ńt F [ h A E g
                        ImVec2 bs = font->CalcTextSizeA(48.0f, FLT_MAX, 0.0f, bbuf);
                        dl->AddText(font, 48.0f, ImVec2(imgPos.x + (viewWidth - bs.x) * 0.5f, imgPos.y + viewHeight * 0.25f),
                            IM_COL32(255, 220, 80, (int)(255 * a)), bbuf);
                    }
                }
            }
            //  G ̒ǐՌo H  3D   e ŕ\  
            {
            XMMATRIX vp = XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj);
            auto project = [&](XMFLOAT3 wpos, ImVec2& out) -> bool {
                XMVECTOR clip = XMVector4Transform(XMVectorSet(wpos.x, wpos.y, wpos.z, 1.0f), vp);
                float ww = XMVectorGetW(clip);
                if (ww <= 0.0001f) return false;
                out.x = imgPos.x + (XMVectorGetX(clip) / ww * 0.5f + 0.5f) * viewWidth;
                out.y = imgPos.y + (1.0f - (XMVectorGetY(clip) / ww * 0.5f + 0.5f)) * viewHeight;
                return true;
                };
            for (const auto& slot : world.Slots()) {
                if (!slot) continue;
                auto* oc = slot->GetComponent<OpponentComponent>();
                if (!oc) continue;
                ImVec2 prev; bool havePrev = false;
                for (const auto& wp : oc->GetPath()) {
                    XMFLOAT3 up = wp; up.y += 0.1f;
                    ImVec2 s; if (!project(up, s)) { havePrev = false; continue; }
                    dl->AddCircleFilled(s, 3.0f, IM_COL32(255, 220, 60, 220));
                    if (havePrev) dl->AddLine(prev, s, IM_COL32(255, 220, 60, 230), 2.0f);
                    prev = s; havePrev = true;
                }
            }
        }

        // Title / Result   ʁiScreenUIComponent j
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            auto* ui = slot->GetComponent<ScreenUIComponent>();
            if (!ui) continue;

            dl->AddRectFilled(imgPos, ImVec2(imgPos.x + viewWidth, imgPos.y + viewHeight), IM_COL32(0, 0, 0, 110));
            ImFont* font = ImGui::GetFont();
            auto centerText = [&](const std::string& txt, float size, float y, ImU32 col) {
                ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, txt.c_str());
                dl->AddText(font, size, ImVec2(imgPos.x + (viewWidth - ts.x) * 0.5f, y), col, txt.c_str());
                };

            float y = imgPos.y + viewHeight * 0.28f;
            centerText(ui->title, 56.0f, y, IM_COL32(255, 255, 255, 255)); y += 72.0f;
            if (ui->showResult && !flow.lastResult.empty()) {
                bool clear = (flow.lastResult == "CLEAR");
                centerText(flow.lastResult, 40.0f, y, clear ? IM_COL32(120, 230, 120, 255) : IM_COL32(255, 90, 90, 255)); y += 52.0f;
                char tbuf[64]; snprintf(tbuf, sizeof(tbuf), "Time  %.1f s", flow.lastTime);
                centerText(tbuf, 22.0f, y, IM_COL32(220, 220, 220, 255)); y += 36.0f;
            }
            if (!ui->subtitle.empty()) { centerText(ui->subtitle, 22.0f, y, IM_COL32(200, 200, 200, 255)); y += 40.0f; }

            if (ui->roleSelect && !ui->nextScene.empty()) {
                //    [   I  F { ^  1 ̑   ɁA3 ̃  [   ̃{ ^      ɕ  ׂ i1-3b j B
                //  L [ { [ h   1 / 2 / 3  ł I ׂ BEnter  ͍  ̃  [   i   { ^   j ̂܂܌   
                const float gap = 24.0f, bh = 56.0f;
                float bw = (viewWidth - gap * (kRoleCount - 1) - 40.0f) / kRoleCount;
                if (bw > 200.0f) bw = 200.0f;
                float x = imgPos.x + (viewWidth - (bw * kRoleCount + gap * (kRoleCount - 1))) * 0.5f;
                int chosen = -1;
                for (int r = 0; r < kRoleCount; ++r, x += bw + gap) {
                    const Role role = (Role)r;
                    const bool current = (role == flow.playerRole);
                    if (current) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 110, 200, 255));
                    ImGui::SetCursorScreenPos(ImVec2(x, y + 10.0f));
                    ImGui::PushID(r);
                    if (ImGui::Button(RoleName(role), ImVec2(bw, bh))) chosen = r;
                    ImGui::PopID();
                    if (current) ImGui::PopStyleColor();
                    //     F { ^   ̉  ɁA { ^   ̕  Ő܂ Ԃ  ďo  
                    dl->AddText(font, 18.0f, ImVec2(x, y + 10.0f + bh + 8.0f), IM_COL32(220, 220, 220, 255), RoleDescription(role), nullptr, bw);
                    if (ImGui::IsKeyPressed('1' + r, false)) chosen = r;
                }
                if (ImGui::IsKeyPressed(0x0D, false)) chosen = (int)flow.playerRole;   // Enter i0x0D = VK_RETURN j
                if (chosen >= 0 && flow.requestedScene.empty()) {
                    flow.playerRole = (Role)chosen;         //  I 񂾃  [     GameFlow  ɓ    i     Scene    ǂށj
                    flow.requestedScene = ui->nextScene;
                }
            }
            else if (!ui->buttonText.empty() && !ui->nextScene.empty()) {
                ImVec2 bs(220.0f, 44.0f);
                ImGui::SetCursorScreenPos(ImVec2(imgPos.x + (viewWidth - bs.x) * 0.5f, y + 10.0f));
                ImGui::PushID(slot.get());
                bool go = ImGui::Button(ui->buttonText.c_str(), bs);
                ImGui::PopID();
                if (ImGui::IsKeyPressed(0x0D, false)) go = true;   // Enter ł    i0x0D = VK_RETURN j
                if (go && flow.requestedScene.empty()) flow.requestedScene = ui->nextScene;
            }
            break;   //   ʂ 1     `  
        }

    }

    //  ҏW   ̂݁F h   b v E L [ E O   b h E M Y   i       j
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
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PREFAB")) {
                std::string pth = payload->Data ? static_cast<const char*>(payload->Data) : "";
                if (!pth.empty()) {
                    EntityId id = EditorSpawnPrefab(world, history, pth);
                    if (id.IsValid()) selected = id;
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
    ID3D11Device* device, const SceneLighting& lighting, const PostProcessSettings& postSettings)
{
    using namespace DirectX;

    // ----    C g   view-proj i   s   e j B e   o  ͈͂ A J     ̑O   ɒǏ]       ----
    XMVECTOR ld = XMVector3Normalize(XMVectorSet(lighting.direction.x, lighting.direction.y, lighting.direction.z, 0));
    XMVECTOR up = (fabsf(XMVectorGetY(ld)) > 0.99f) ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);

    //  ͈͂̒  S F J     ̈ʒu    A     ȑO   ֏    i ߂  _ i     O  L     j
    const float range = (lighting.shadowRange > 1.0f) ? lighting.shadowRange : 1.0f;   //  e   o 鐳   ` ̈ Ӂim j
    XMMATRIX camWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));               // r[3]= J     ʒu, r[2]= O  
    XMVECTOR forward = XMVectorSetY(camWorld.r[2], 0.0f);
    if (XMVectorGetX(XMVector3LengthSq(forward)) > 1e-6f) forward = XMVector3Normalize(forward);
    else forward = XMVectorZero();                                                     //  ^  E ^        Ă   Ƃ 
    XMVECTOR center = XMVectorAdd(camWorld.r[3], XMVectorScale(forward, range * 0.35f));

    //    S  u V   h E } b v1  f Ԃ v ̍  ݂Ɋۂ߂ B
    //  ۂ߂Ȃ  ƁA J                 тɉe ̉         i  f ̋ ؂ ̈ʒu     t   [      邽 ߁j B
    XMMATRIX lightRot = XMMatrixLookAtLH(XMVectorZero(), ld, up);                      //          ̃  C g s  
    const float texel = range / (float)shadowMap.GetSize();
    XMFLOAT3 c; XMStoreFloat3(&c, XMVector3TransformCoord(center, lightRot));          //    C g   猩     W  
    c.x = floorf(c.x / texel) * texel;
    c.y = floorf(c.y / texel) * texel;
    center = XMVector3TransformCoord(XMLoadFloat3(&c), XMMatrixInverse(nullptr, lightRot));   //    [   h ֖߂ 

    XMVECTOR lpos = XMVectorSubtract(center, XMVectorScale(ld, 25.0f));                //    S      Ƌt       25m
    XMMATRIX lView = XMMatrixLookAtLH(lpos, center, up);
    XMMATRIX lProj = XMMatrixOrthographicLH(range, range, 0.1f, 60.0f);
    XMFLOAT4X4 lightVP; XMStoreFloat4x4(&lightVP, lView * lProj);

    //  V   h E p X
    shadowMap.BeginPass(context, lightVP);
    RenderSystem::RenderShadow(world, shadowMap, *cubeMesh, cache, device, context);
    RenderSystem::RenderShadowSkinned(world, shadowMap, GlobalSkinnedCache(), device, context);
    //  { `  
    rt.Begin(context, 0.10f, 0.12f, 0.15f, 1.0f);
    if (rt.GetWidth() <= 0 || rt.GetHeight() <= 0 || !cubeMesh) return;

    XMFLOAT4X4 viewProj;
    XMStoreFloat4x4(&viewProj, XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));
    XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));
    XMFLOAT3 camPos; XMStoreFloat3(&camPos, invView.r[3]);

    skybox.Render(context, viewProj, camPos, envMap.Sky(), lighting);
    //   ͐F      `   B         i   b V   j ́A F ɉ    Ė@        o  
    rt.BindSceneTargets(context);
    shader.SetLight(lighting);
    shader.SetShadow(shadowMap.GetSRV(), lightVP);
    shader.SetEnvironment(envMap.Specular(), envMap.BrdfLut());
    shader.SetCameraPosition(camPos);
    RenderSystem::Render(world, shader, *cubeMesh, cache, device, context, viewProj);
    skinnedShader.SetLight(lighting);
    skinnedShader.SetShadow(shadowMap.GetSRV(), lightVP);
    skinnedShader.SetEnvironment(envMap.Specular(), envMap.BrdfLut());
    skinnedShader.SetCameraPosition(camPos);
    RenderSystem::RenderSkinned(world, skinnedShader, GlobalSkinnedCache(), device, context, viewProj);
    //  V   h ESRV  O   i   t   [   ̃V   h E       ݂Ƌ        Ȃ  j
    ID3D11ShaderResourceView* nul = nullptr;
    context->PSSetShaderResources(1, 1, &nul);

    //  | X g v   Z X F V [   ̉摜 irt j  ǂ ŁA \   p ̉摜 ifinalRT j ɏ    o  
            //  | X g v   Z X F V [   ̉摜 irt  ̐F E @   E [ x j  ǂ ŁA \   p ̉摜 ifinalRT j ɏ    o  
    postProcess.Apply(context, rt, finalRT, postSettings, proj);
}

} // namespace uel