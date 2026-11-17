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
#include "Engine/Components/RoleComponent.h"
#include "Engine/Components/PartySetupComponent.h"
#include "Engine/Components/LifeStateComponent.h"

#include <cfloat>
#include <cmath>
#include <d3d11.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include <string>
#include <cstdio>
namespace uel {
    namespace {

        // Play 中のマウスの固定（3-1）で使う、Win32 の補助（Windows の関数は d3d11.h が読み込む windows.h のもの）。
        // このアプリのウィンドウ（ImGui のメインのビューポートが持っている HWND）
        HWND MainWindowHandle()
        {
            return static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
        }

        // ImGui の座標（ウィンドウの中の位置。マルチビューポートを使っていないので、クライアント座標と同じ）→ 画面の座標
        POINT ClientToScreenPoint(HWND hwnd, float x, float y)
        {
            POINT p{ static_cast<LONG>(x), static_cast<LONG>(y) };
            ::ClientToScreen(hwnd, &p);
            return p;
        }

    } // namespace

    void ViewportPanel::Initialize(ID3D11Device* device)
    {
        rt.Create(device, viewWidth, viewHeight, true);        // シーン用：色＋法線＋深度
        finalRT.Create(device, viewWidth, viewHeight, false);  // 色だけ
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
        // 新しく作ってみて、できたものだけ入れ替える（作れなかったら前のシェーダのまま）。
        // ライト・影・景色・ポストプロセスの設定は、毎フレーム RenderScene で渡し直すので、作り直しても消えない。
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
            modeButton(u8"移動", ImGuizmo::TRANSLATE); ImGui::SameLine();
            modeButton(u8"回転", ImGuizmo::ROTATE);    ImGui::SameLine();
            modeButton(u8"拡大縮小", ImGuizmo::SCALE);
            ImGui::SameLine(); ImGui::Checkbox(u8"グリッド", &showGrid);
            ImGui::SameLine(); ImGui::Checkbox(u8"スナップ", &snapEnabled);
            ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##snapT", &snapTranslate, 0.1f, 0.01f, 100.0f, "T:%.2f");
            ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##snapR", &snapRotate, 1.0f, 1.0f, 180.0f, "R:%.0f");
            ImGui::SameLine(); ImGui::SetNextItemWidth(70); ImGui::DragFloat("##snapS", &snapScale, 0.05f, 0.01f, 10.0f, "S:%.2f");
            ImGui::SameLine(); ImGui::Checkbox(u8"当たり判定", &showHitboxes);
        }
        else {
            ImGui::TextUnformatted(u8"プレイ中（クリック：操作を始める　Esc：マウスを離す　マウス：視点　WASD：移動　Space：ジャンプ　左クリック：射撃　R：リロード　右クリック：回復（Support）　E 長押し：仲間を起こす）");
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

            // ---- マウスの固定（3-1。UE の Play 中の「マウスをゲームが持つ」＝Mouse Capture）----
            // Viewport の画像をクリックすると、カーソルを隠して画像の中に閉じ込め、毎フレーム真ん中へ戻す。
            // 真ん中から動いた量が、そのまま視点の回転になる（右ボタンを押さなくても視点が回る。右クリックは空く）。
            // 離すのは：Esc を押した／プレイヤーがいない Scene（Title・Result など。ボタンを押せるように）／
            //           ほかのアプリへ切り替えた（Alt+Tab）／Play をやめた（下の else の中）
            const ImVec2 imgMin = ImGui::GetCursorScreenPos();     // このあと描く画像の左上（ここから画像までの間に部品は置かない）
            const ImVec2 imgMax(imgMin.x + viewWidth, imgMin.y + viewHeight);
            HWND hwnd = MainWindowHandle();
            bool justCaptured = false;      // このフレームに固定した（固定したクリックでは撃たない）
            if (!mouseCaptured) {
                const bool inImage = io.MousePos.x >= imgMin.x && io.MousePos.x < imgMax.x
                    && io.MousePos.y >= imgMin.y && io.MousePos.y < imgMax.y;
                if (player && hwnd && ImGui::IsWindowHovered() && inImage && ImGui::IsMouseClicked(0)) {
                    mouseCaptured = true;
                    justCaptured = true;
                    captureClickHeld = true;    // このクリックを離すまでは、押しっぱなしでも撃たない（3-3）
                }
            }
            else if (!player || !hwnd || ImGui::IsKeyPressed(0x1B, false) || ::GetForegroundWindow() != hwnd) {   // 0x1B = VK_ESCAPE
                mouseCaptured = false;
                ::ClipCursor(nullptr);
            }
            float lookDX = 0.0f, lookDY = 0.0f;
            if (mouseCaptured) {
                const POINT center = ClientToScreenPoint(hwnd, (imgMin.x + imgMax.x) * 0.5f, (imgMin.y + imgMax.y) * 0.5f);
                POINT cur;
                if (!justCaptured && ::GetCursorPos(&cur)) {        // 前のフレームの最後に真ん中へ戻したので、ずれた分が動いた量
                    lookDX = static_cast<float>(cur.x - center.x);
                    lookDY = static_cast<float>(cur.y - center.y);
                }
                // 画像の外へ出さない（ほかのパネルやメニューを押さない）。ウィンドウを動かしても合うように、毎フレーム設定し直す
                const POINT a = ClientToScreenPoint(hwnd, imgMin.x, imgMin.y);
                const POINT b = ClientToScreenPoint(hwnd, imgMax.x, imgMax.y);
                const RECT clip{ a.x, a.y, b.x, b.y };
                ::ClipCursor(&clip);
                ::SetCursorPos(center.x, center.y);
                ImGui::SetMouseCursor(ImGuiMouseCursor_None);      // カーソルを隠す（毎フレーム指定する。ImGui がフレームの頭で矢印に戻すため）
            }

            InputState in;
            in.enabled = actv && !io.WantTextInput;
            in.keyW = ImGui::IsKeyDown('W'); in.keyS = ImGui::IsKeyDown('S');
            in.keyD = ImGui::IsKeyDown('D'); in.keyA = ImGui::IsKeyDown('A');
            in.jumpPressed = ImGui::IsKeyPressed(' ', false);
            if (!io.MouseDown[0]) captureClickHeld = false;     // 固定したときのクリックを離した
            in.shootClicked = mouseCaptured && !justCaptured && ImGui::IsMouseClicked(0);   // 固定したときのクリックでは撃たない
            in.shootHeld = mouseCaptured && !captureClickHeld && io.MouseDown[0];           // 左クリックを押している間（連射。3-3）
            in.reloadPressed = mouseCaptured && ImGui::IsKeyPressed('R', false);            // R でリロード（3-3）
            in.healHeld = mouseCaptured && io.MouseDown[1];                                 // 右クリックを押している間（Support の回復。3-2）
            in.interactHeld = mouseCaptured && ImGui::IsKeyDown('E');                       // E を押している間（ダウンした味方の蘇生。4-2）
            in.lookActive = mouseCaptured;                  // 固定している間は、いつでも視点が回る
            in.mouseDX = lookDX; in.mouseDY = lookDY;
            float dt = ClampDeltaTime(io.DeltaTime);     // 上限付き（EngineConfig.h）

            // 添字でループ（Spawnerが更新中にEntityを増やしても壊れないように）
            for (size_t i = 0; i < world.Slots().size(); ++i) {
                Entity* e = world.Slots()[i].get();
                if (!e) continue;
                GameContext ctx{ world, *e, cache, device, dt, in, &nav, &flow };
                for (size_t ci = 0; ci < e->components.size(); ++ci) e->components[ci]->Update(ctx);
            }
            world.FlushDestroy();     // Update の中で予約された「後で消す」を、ここでまとめて消す
            if (player) { view = player->GetComponent<PlayerComponent>()->GetView(); }
            else { view = camera.GetView(); }
        }
        else {
            if (mouseCaptured) {            // Play をやめた：マウスの固定を外す（3-1）
                mouseCaptured = false;
                ::ClipCursor(nullptr);
            }
            camera.Update(ImGui::IsWindowHovered());
            view = camera.GetView();
        }
        XMStoreFloat4x4(&proj, XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f));

        ImVec2 imgPos = ImGui::GetCursorScreenPos();
        if (finalRT.GetSRV())
            ImGui::Image((ImTextureID)finalRT.GetSRV(), ImVec2((float)viewWidth, (float)viewHeight));
        // ヒットボックスの表示（編集中は選択中のみ／「Hitbox」チェックで全員。Play中も有効）
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
            // 円（plane 0:水平XZ / 1:XY / 2:ZY）
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
                // Spawnerの湧き範囲（編集中に選択している時だけ、黄色の円

            }
            if (!playMode) {
                if (Entity* se = world.Get(selected)) {
                    auto* t = se->GetComponent<TransformComponent>();
                    auto* sp = se->GetComponent<SpawnerComponent>();
                    if (t && sp) circle(t->position, sp->spawnRadius, 0, IM_COL32(255, 210, 60, 230));
                }
            }
        }
        // Play中のオーバーレイ（照準・ヒットマーカー・HP・敵経路・Title/Result画面）
        if (playMode) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 c(imgPos.x + viewWidth * 0.5f, imgPos.y + viewHeight * 0.5f);

            Entity* p = nullptr;
            for (const auto& slot : world.Slots())
                if (slot && slot->GetComponent<PlayerComponent>()) { p = slot.get(); break; }
            PlayerComponent* pc = p ? p->GetComponent<PlayerComponent>() : nullptr;

            if (pc) {   // 照準はプレイヤーがいる時だけ
                dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), IM_COL32(255, 255, 255, 220), 2.0f);
                dl->AddLine(ImVec2(c.x, c.y - 8), ImVec2(c.x, c.y + 8), IM_COL32(255, 255, 255, 220), 2.0f);
            }
            // 明るい空や床の上でも読めるように、文字の右下に黒い影を付けて描く
            auto shadowText = [&](ImVec2 pos, ImU32 col, const char* txt) {
                dl->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 200), txt);
                dl->AddText(pos, col, txt);
                };
            // プレイヤーがダウンしている間（4-4）：画面を暗い赤で覆い、四辺をさらに暗くする（ほかの HUD より先に描いて、文字は上に出す）
            if (p) if (const LifeStateComponent* pls = p->GetComponent<LifeStateComponent>()) if (pls->IsDowned()) {
                const ImVec2 a = imgPos, b(imgPos.x + viewWidth, imgPos.y + viewHeight);
                const ImU32 dark = IM_COL32(40, 0, 0, 200), clear = IM_COL32(40, 0, 0, 0);
                const float band = (std::min)(viewWidth, viewHeight) * 0.25f;
                dl->AddRectFilled(a, b, IM_COL32(70, 0, 0, 80));
                dl->AddRectFilledMultiColor(a, ImVec2(b.x, a.y + band), dark, dark, clear, clear);    // 上
                dl->AddRectFilledMultiColor(ImVec2(a.x, b.y - band), b, clear, clear, dark, dark);    // 下
                dl->AddRectFilledMultiColor(a, ImVec2(a.x + band, b.y), dark, clear, clear, dark);    // 左
                dl->AddRectFilledMultiColor(ImVec2(b.x - band, a.y), b, clear, dark, dark, clear);    // 右
            }
            if (pc && !mouseCaptured) {     // マウスを固定していない：クリックで始められることを、照準の下に出す（3-1）
                const char* hint = u8"クリックで操作を始める（Esc でマウスを離す）";
                const ImVec2 ts = ImGui::CalcTextSize(hint);
                shadowText(ImVec2(c.x - ts.x * 0.5f, c.y + 24.0f), IM_COL32(255, 255, 255, 240), hint);
            }
            if (pc) {   // 選んだロール（左上。GameFlow に入っている値をそのまま出す）
                char roleTxt[32]; snprintf(roleTxt, sizeof(roleTxt), "ROLE: %s", RoleName(flow.playerRole));
                shadowText(ImVec2(imgPos.x + 20.0f, imgPos.y + 12.0f), IM_COL32(255, 255, 255, 240), roleTxt);
            }
            // 蘇生の進み具合のバー（4-2）：真ん中に横長のバーと「REVIVING」。y はバーの上端
            auto reviveBar = [&](float y, float progress) {
                const float bw = 220.0f, bh = 10.0f, bx = c.x - bw * 0.5f;
                if (progress < 0.0f) progress = 0.0f; if (progress > 1.0f) progress = 1.0f;
                dl->AddRectFilled(ImVec2(bx, y), ImVec2(bx + bw, y + bh), IM_COL32(40, 40, 40, 210));
                dl->AddRectFilled(ImVec2(bx, y), ImVec2(bx + bw * progress, y + bh), IM_COL32(120, 230, 140, 240));
                dl->AddRect(ImVec2(bx, y), ImVec2(bx + bw, y + bh), IM_COL32(255, 255, 255, 180));
                const ImVec2 rs = ImGui::CalcTextSize("REVIVING");
                shadowText(ImVec2(c.x - rs.x * 0.5f, y - rs.y - 2.0f), IM_COL32(120, 230, 140, 255), "REVIVING");
                };
            // プレイヤーが蘇生している：照準の下にバー。近くにダウンした味方がいる：照準の下に「E 長押し：仲間を起こす」
            if (pc && mouseCaptured) {
                if (pc->IsReviving()) reviveBar(c.y + 90.0f, pc->GetReviveProgress());
                else if (pc->CanRevive()) {
                    const char* hint = u8"E 長押し：仲間を起こす";
                    const ImVec2 ts = ImGui::CalcTextSize(hint);
                    shadowText(ImVec2(c.x - ts.x * 0.5f, c.y + 40.0f), IM_COL32(120, 230, 140, 255), hint);
                }
            }
            if (pc && pc->GetHitMarker() > 0.0f) {
                ImU32 col = pc->GetHeadshot() ? IM_COL32(255, 60, 60, 255) : IM_COL32(255, 255, 255, 255);   // 頭=赤／胴体=白
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
                // ダウン（段階 4）：画面の真ん中の上に、大きく「DOWNED」と残りの秒数。
                // Dead・状態を持たないときの HP 0 は、今までどおり「YOU DIED」
                const LifeStateComponent* pls = p->GetComponent<LifeStateComponent>();
                if (pls && pls->IsDowned()) {
                    ImFont* font = ImGui::GetFont();
                    char dTxt[32]; snprintf(dTxt, sizeof(dTxt), "DOWNED  %d", (int)std::ceil(pls->GetDownTimeLeft()));
                    const float size = 40.0f;
                    const ImVec2 ds = font->CalcTextSizeA(size, FLT_MAX, 0.0f, dTxt);
                    const ImVec2 dp(c.x - ds.x * 0.5f, c.y - 110.0f);
                    dl->AddText(font, size, ImVec2(dp.x + 2.0f, dp.y + 2.0f), IM_COL32(0, 0, 0, 200), dTxt);   // 影
                    dl->AddText(font, size, dp, IM_COL32(255, 90, 70, 255), dTxt);
                    const char* wait = u8"仲間の助けを待っています";
                    const ImVec2 ws = ImGui::CalcTextSize(wait);
                    shadowText(ImVec2(c.x - ws.x * 0.5f, dp.y + ds.y + 4.0f), IM_COL32(255, 255, 255, 240), wait);
                    // 仲間に起こしてもらっている間は、照準の下に進み具合のバー（4-2。ダウンの時間はその間止まる）
                    if (pls->IsBeingRevived()) reviveBar(c.y + 90.0f, pls->GetReviveProgress());
                }
                else if (php->hp <= 0.0f) dl->AddText(ImVec2(c.x - 45, c.y - 40), IM_COL32(255, 80, 80, 255), "YOU DIED");
            }
            // 弾数（右下。3-3）：「残り / 装弾数」を大きく。残りが 1/4 以下で橙、0 で赤。
            // リロード中は数字の上に「RELOADING」と、進み具合のバー
            if (pc && pc->GetMagazineSize() > 0) {
                ImFont* font = ImGui::GetFont();
                const float size = 32.0f;
                char ammoTxt[32]; snprintf(ammoTxt, sizeof(ammoTxt), "%d / %d", pc->GetAmmo(), pc->GetMagazineSize());
                const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, ammoTxt);
                const ImVec2 pos(imgPos.x + viewWidth - 24.0f - ts.x, imgPos.y + viewHeight - 24.0f - ts.y);
                const int ammo = pc->GetAmmo(), mag = pc->GetMagazineSize();
                const ImU32 col = (ammo <= 0) ? IM_COL32(255, 80, 80, 255)
                    : (ammo * 4 <= mag) ? IM_COL32(255, 180, 60, 255) : IM_COL32(255, 255, 255, 255);
                dl->AddText(font, size, ImVec2(pos.x + 2.0f, pos.y + 2.0f), IM_COL32(0, 0, 0, 200), ammoTxt);  // 影
                dl->AddText(font, size, pos, col, ammoTxt);
                const float reload = pc->GetReloadProgress();
                if (reload >= 0.0f) {
                    const float bw = 140.0f, bh = 6.0f;
                    const float bx = imgPos.x + viewWidth - 24.0f - bw, by = pos.y - 10.0f;
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(40, 40, 40, 200));
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * reload, by + bh), IM_COL32(255, 220, 120, 240));
                    const ImVec2 rs = ImGui::CalcTextSize("RELOADING");
                    shadowText(ImVec2(bx + bw - rs.x, by - rs.y - 2.0f), IM_COL32(255, 220, 120, 255), "RELOADING");
                }
            }
            // AI 仲間の HP（プレイヤーの HP バーの上に、1人ずつ小さく並べる）
            {
                const float bx = imgPos.x + 20.0f, bw = 160.0f, bh = 12.0f;
                float by = imgPos.y + viewHeight - 62.0f;
                for (const auto& slot : world.Slots()) {
                    if (!slot) continue;
                    auto* al = slot->GetComponent<AllyComponent>();
                    if (!al) continue;
                    auto* ahp = slot->GetComponent<HPComponent>();
                    if (!ahp) continue;
                    float frac = (ahp->maxHp > 0.0f) ? ahp->hp / ahp->maxHp : 0.0f;
                    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(40, 40, 40, 200));
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * frac, by + bh), IM_COL32(80, 200, 120, 230));
                    dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(255, 255, 255, 160));
                    // 名前と HP の後ろに、弾の残り（リロード中は RELOAD。3-5）。
                    // ダウン中は「DOWNED 残りの秒数」（赤）、Dead は「DEAD」（灰色）（段階 4）
                    char aTxt[128];
                    ImU32 aCol = IM_COL32(255, 255, 255, 240);
                    const LifeStateComponent* als = slot->GetComponent<LifeStateComponent>();
                    if (als && als->IsBeingRevived()) {     // 起こしてもらっている（4-2）：進み具合（緑）
                        snprintf(aTxt, sizeof(aTxt), "%s  REVIVING %d%%", slot->name.c_str(), (int)(als->GetReviveProgress() * 100.0f));
                        aCol = IM_COL32(120, 230, 140, 255);
                    }
                    else if (als && als->IsDowned()) {
                        snprintf(aTxt, sizeof(aTxt), "%s  DOWNED %d", slot->name.c_str(), (int)std::ceil(als->GetDownTimeLeft()));
                        aCol = IM_COL32(255, 110, 90, 255);
                    }
                    else if (als && als->IsDead()) {
                        snprintf(aTxt, sizeof(aTxt), "%s  DEAD", slot->name.c_str());
                        aCol = IM_COL32(160, 160, 160, 230);
                    }
                    else if (al->IsReloading()) snprintf(aTxt, sizeof(aTxt), "%s  %.0f   RELOAD", slot->name.c_str(), ahp->hp);
                    else                        snprintf(aTxt, sizeof(aTxt), "%s  %.0f   %d / %d", slot->name.c_str(), ahp->hp, al->GetAmmo(), al->GetMagazineSize());
                    shadowText(ImVec2(bx + bw + 8.0f, by - 3.0f), aCol, aTxt);
                    by -= 20.0f;
                }
            }
            // ウェーブ表示（Spawnerがある時）
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

                    if (sp->BannerTime() > 0.0f) {                     // ウェーブ開始の大きな表示
                        char bbuf[32]; snprintf(bbuf, sizeof(bbuf), "WAVE %d", sp->CurrentWave());
                        ImFont* font = ImGui::GetFont();
                        float a = (sp->BannerTime() > 1.0f) ? 1.0f : sp->BannerTime();   // 最後の1秒でフェードアウト
                        ImVec2 bs = font->CalcTextSizeA(48.0f, FLT_MAX, 0.0f, bbuf);
                        dl->AddText(font, 48.0f, ImVec2(imgPos.x + (viewWidth - bs.x) * 0.5f, imgPos.y + viewHeight * 0.25f),
                            IM_COL32(255, 220, 80, (int)(255 * a)), bbuf);
                    }
                }
            }
            // 敵の追跡経路と、AI 仲間の弾の線・回復の線・名札を3D投影で表示
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

                // AI 仲間の弾の線（2-2b）：撃った直後の短い間（AllyComponent の GetShotTime() が 0 より大きい間）だけ、
                // 撃った位置から当たった所（外れたら射程の先）まで線を引く（UE なら弾道のエフェクト＝Tracer に当たる）。
                // AI 仲間はプレイヤーの後ろにいることが多く、線の始まりがカメラの後ろになりやすい。そのまま写すと線が消えるか
                // 裏返るので、カメラのすぐ前の面（w = kNearW）で線を切ってから画面に写す（描画の「ニアクリップ」と同じ考え方）。
                // ImGui で画像の上に描くので、壁の後ろでも隠れない（撃つのは見えている敵だけなので、ほとんど目立たない）
                auto projectSegment = [&](const XMFLOAT3& a, const XMFLOAT3& b, ImVec2& sa, ImVec2& sb) -> bool {
                    const float kNearW = 0.1f;      // カメラの near と同じ
                    XMVECTOR ca = XMVector4Transform(XMVectorSet(a.x, a.y, a.z, 1.0f), vp);
                    XMVECTOR cb = XMVector4Transform(XMVectorSet(b.x, b.y, b.z, 1.0f), vp);
                    const float wa = XMVectorGetW(ca), wb = XMVectorGetW(cb);
                    if (wa < kNearW && wb < kNearW) return false;                                   // 両方カメラの後ろ
                    if (wa < kNearW) ca = XMVectorLerp(ca, cb, (kNearW - wa) / (wb - wa));          // a 側を切る
                    else if (wb < kNearW) cb = XMVectorLerp(cb, ca, (kNearW - wb) / (wa - wb));     // b 側を切る
                    auto toScreen = [&](FXMVECTOR c) {
                        const float w = XMVectorGetW(c);
                        return ImVec2(imgPos.x + (XMVectorGetX(c) / w * 0.5f + 0.5f) * viewWidth,
                            imgPos.y + (1.0f - (XMVectorGetY(c) / w * 0.5f + 0.5f)) * viewHeight);
                        };
                    sa = toScreen(ca);
                    sb = toScreen(cb);
                    return true;
                    };
                // 回復の線（2-3b）：Support の AI 仲間が回復している間ずっと、自分から相手の胴体まで緑の線を引く
                // （AllyComponent の GetHealFrom / GetHealTo。位置は AllyComponent が毎フレーム更新している）。
                // つながっていると分かるように、明るさを少し脈打たせる（ImGui の時刻から計算。1フレームに sin 1回だけ）。
                // 自分を回復しているときは線にならないので、自分の胸の高さに緑の輪を出す
                const float healPulse = 0.75f + 0.25f * sinf((float)ImGui::GetTime() * 10.0f);
                const ImU32 healCol = IM_COL32(90, 255, 140, (int)(255.0f * healPulse));
                bool playerHealed = false;      // プレイヤーが回復されている（HP バーの横に「+ HEAL」を出す）
                for (const auto& slot : world.Slots()) {
                    if (!slot) continue;
                    auto* al = slot->GetComponent<AllyComponent>();
                    if (!al) continue;
                    if (al->IsHealing()) {
                        if (al->GetHealTarget() == slot->id) {              // 自分を回復：自分の胸の高さ（線の始まり）に輪
                            ImVec2 s;
                            if (project(al->GetHealFrom(), s)) {
                                dl->AddCircle(s, 14.0f, IM_COL32(0, 0, 0, 140), 24, 5.0f);
                                dl->AddCircle(s, 14.0f, healCol, 24, 2.5f);
                            }
                        }
                        else {
                            if (p && al->GetHealTarget() == p->id) playerHealed = true;
                            ImVec2 sa, sb;
                            if (projectSegment(al->GetHealFrom(), al->GetHealTo(), sa, sb)) {
                                dl->AddLine(sa, sb, IM_COL32(0, 0, 0, 140), 6.0f);  // 黒い縁（弾の線より太い）
                                dl->AddLine(sa, sb, healCol, 3.0f);
                            }
                        }
                    }
                    if (al->GetShotTime() > 0.0f) {
                        ImVec2 sa, sb;
                        if (projectSegment(al->GetShotFrom(), al->GetShotTo(), sa, sb)) {
                            // 黒い縁の上に明るい線を重ねる（空の上でも白い床の上でも見え、敵の経路の黄色い線とも見分けられる）。
                            // 名札より先に描くので、名札が上に出る
                            dl->AddLine(sa, sb, IM_COL32(0, 0, 0, 140), 4.0f);
                            dl->AddLine(sa, sb, IM_COL32(255, 245, 200, 255), 2.0f);
                        }
                    }
                }
                // プレイヤーが回復されているとき：線はカメラの中へ入っていくので分かりにくい。HP バーの右に「+ HEAL」を出す
                if (playerHealed)
                    shadowText(ImVec2(imgPos.x + 20.0f + 220.0f + 10.0f, imgPos.y + viewHeight - 40.0f), IM_COL32(90, 255, 140, 255), "+ HEAL");   // 文字は脈打たせない（読みやすさ優先）

                // プレイヤー（Support）の回復の線（3-2）：一人称なので、線の始まりは画面の下の右寄り（手に持った物から出ている見せ方）。
                // 終わりは相手の胴体。AI 仲間の回復の線と同じ色・同じ脈打ち方で、少し太くする（手前にあるので）
                if (pc && pc->IsHealing()) {
                    ImVec2 sb;
                    if (project(pc->GetHealTo(), sb)) {
                        const ImVec2 sa(imgPos.x + viewWidth * 0.68f, imgPos.y + viewHeight);
                        dl->AddLine(sa, sb, IM_COL32(0, 0, 0, 140), 8.0f);
                        dl->AddLine(sa, sb, healCol, 4.0f);
                    }
                }

                // AI 仲間の名札：頭の少し上に、ロールの名前をロールの色で出す（1-4d）。
                // 壁の向こうでも見える（味方の位置が分かるように。FPS の味方の名前表示でよくある作り）。戦闘不能なら灰色
                bool partyError = false;
                for (const auto& slot : world.Slots()) {
                    if (!slot) continue;
                    if (auto* ps = slot->GetComponent<PartySetupComponent>()) if (ps->HasError()) partyError = true;
                    auto* al = slot->GetComponent<AllyComponent>();
                    auto* rc = slot->GetComponent<RoleComponent>();
                    auto* t = slot->GetComponent<TransformComponent>();
                    if (!al || !rc || !t) continue;
                    // 頭の上端：Hitbox があれば頭の球の上、無ければ 2m（どちらも Scale.y を掛ける）
                    float top = 2.0f * t->scale.y;
                    if (auto* hb = slot->GetComponent<HitboxComponent>()) top = (hb->headHeight + hb->headRadius) * t->scale.y;
                    ImVec2 s;
                    if (!project(XMFLOAT3{ t->position.x, t->position.y + top + 0.25f, t->position.z }, s)) continue;   // カメラの後ろ
                    if (s.x < imgPos.x || s.x > imgPos.x + viewWidth || s.y < imgPos.y || s.y > imgPos.y + viewHeight) continue;   // 画面の外
                    ImU32 col = IM_COL32(120, 230, 140, 255);                                   // Support：緑
                    if (rc->GetRole() == Role::Tank) col = IM_COL32(110, 170, 255, 255);        // Tank：青
                    else if (rc->GetRole() == Role::DPS) col = IM_COL32(255, 150, 90, 255);     // DPS：オレンジ
                    if (al->IsIncapacitated()) col = IM_COL32(160, 160, 160, 220);
                    const char* name = RoleName(rc->GetRole());
                    const ImVec2 ts = ImGui::CalcTextSize(name);
                    shadowText(ImVec2(s.x - ts.x * 0.5f, s.y - ts.y), col, name);
                    if (const LifeStateComponent* ls = slot->GetComponent<LifeStateComponent>()) if (ls->IsDead()) {   // Dead：名前の下に DEAD（4-4）
                        const ImVec2 ds = ImGui::CalcTextSize("DEAD");
                        shadowText(ImVec2(s.x - ds.x * 0.5f, s.y + 2.0f), IM_COL32(160, 160, 160, 220), "DEAD");
                    }
                }

                // ダウンした味方の印（4-4）：脈打つ赤い丸と「+」、その下に残りの秒数と距離。起こしてもらっている間は緑で進み具合。
                // 壁の向こうでも見える。画面の外・カメラの後ろにいるときは、その方向の画面の端に出す（どこへ助けに行けばよいか分かるように。
                // L4D2 の「倒れた仲間の輪郭が壁越しに見える」に当たる）。プレイヤー自身には出さない
                if (pc) {
                    const XMMATRIX viewM = XMLoadFloat4x4(&view);
                    const float inset = 48.0f;
                    const ImVec2 rectMin(imgPos.x + inset, imgPos.y + inset);
                    const ImVec2 rectMax(imgPos.x + viewWidth - inset, imgPos.y + viewHeight - 130.0f);    // 下は HP・弾数の欄に重ならないように
                    const float pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 6.0f);
                    const TransformComponent* myT = p->GetComponent<TransformComponent>();
                    for (const auto& slot : world.Slots()) {
                        if (!slot || slot.get() == p) continue;
                        const LifeStateComponent* ls = slot->GetComponent<LifeStateComponent>();
                        const TransformComponent* t = slot->GetComponent<TransformComponent>();
                        if (!ls || !ls->IsDowned() || !t) continue;
                        const XMFLOAT3 wp{ t->position.x, t->position.y + 0.6f * t->scale.y, t->position.z };     // 倒れた体のあたり
                        // 画面の中に写るならそこ。写らなければ、カメラから見た向き（左右・上下）の画面の端
                        ImVec2 s;
                        XMFLOAT3 vp3;
                        XMStoreFloat3(&vp3, XMVector3TransformCoord(XMLoadFloat3(&wp), viewM));
                        const bool onScreen = vp3.z > 0.1f && project(wp, s)
                            && s.x >= rectMin.x && s.x <= rectMax.x && s.y >= rectMin.y && s.y <= rectMax.y;
                        if (!onScreen) {
                            float dx = vp3.x, dy = -vp3.y;                          // 画面は下が +
                            if (dx * dx + dy * dy < 1e-6f) { dx = 0.0f; dy = 1.0f; }
                            const float hw = (rectMax.x - rectMin.x) * 0.5f, hh = (rectMax.y - rectMin.y) * 0.5f;
                            const float k = (std::min)(std::fabs(dx) > 1e-6f ? hw / std::fabs(dx) : 1e9f,
                                std::fabs(dy) > 1e-6f ? hh / std::fabs(dy) : 1e9f);
                            s = ImVec2((rectMin.x + rectMax.x) * 0.5f + dx * k, (rectMin.y + rectMax.y) * 0.5f + dy * k);   // 枠の真ん中から
                        }
                        const bool reviving = ls->IsBeingRevived();
                        const ImU32 ring = reviving ? IM_COL32(120, 230, 140, 255) : IM_COL32(255, 70, 60, (int)(150 + 105 * pulse));
                        const float r = reviving ? 14.0f : 12.0f + 4.0f * pulse;
                        dl->AddCircleFilled(s, r + 2.0f, IM_COL32(0, 0, 0, 160), 24);
                        dl->AddCircleFilled(s, r, ring, 24);
                        dl->AddLine(ImVec2(s.x - 6.0f, s.y), ImVec2(s.x + 6.0f, s.y), IM_COL32(255, 255, 255, 255), 3.0f);    // 「+」
                        dl->AddLine(ImVec2(s.x, s.y - 6.0f), ImVec2(s.x, s.y + 6.0f), IM_COL32(255, 255, 255, 255), 3.0f);
                        char mTxt[48];
                        const float dist = myT ? std::sqrt((t->position.x - myT->position.x) * (t->position.x - myT->position.x)
                            + (t->position.z - myT->position.z) * (t->position.z - myT->position.z)) : 0.0f;
                        if (reviving) snprintf(mTxt, sizeof(mTxt), "REVIVING %d%%", (int)(ls->GetReviveProgress() * 100.0f));
                        else          snprintf(mTxt, sizeof(mTxt), "DOWNED %d  %.0fm", (int)std::ceil(ls->GetDownTimeLeft()), dist);
                        const ImVec2 ms = ImGui::CalcTextSize(mTxt);
                        float mx = s.x - ms.x * 0.5f;                                           // 文字が画面の端で切れないように
                        mx = (std::max)(imgPos.x + 6.0f, (std::min)(mx, imgPos.x + viewWidth - ms.x - 6.0f));
                        shadowText(ImVec2(mx, s.y + r + 4.0f), reviving ? IM_COL32(120, 230, 140, 255) : IM_COL32(255, 110, 90, 255), mTxt);
                    }
                }
                if (partyError)     // AI 仲間の Prefab が読めなかった（PartySetup）
                    shadowText(ImVec2(imgPos.x + 20.0f, imgPos.y + 34.0f), IM_COL32(255, 120, 120, 255), "PartySetup: ally prefab not found");
            }

            // Title / Result 画面（ScreenUIComponent）
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
                    // ロール選択：ボタン1つの代わりに、3つのロールのボタンを横に並べる（1-3b）。
                    // キーボードの 1 / 2 / 3 でも選べる。Enter は今のロール（青いボタン）のまま決定
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
                        // 説明：ボタンの下に、ボタンの幅で折り返して出す
                        dl->AddText(font, 18.0f, ImVec2(x, y + 10.0f + bh + 8.0f), IM_COL32(220, 220, 220, 255), RoleDescription(role), nullptr, bw);
                        if (ImGui::IsKeyPressed('1' + r, false)) chosen = r;
                    }
                    if (ImGui::IsKeyPressed(0x0D, false)) chosen = (int)flow.playerRole;   // Enter（0x0D = VK_RETURN）
                    if (chosen >= 0 && flow.requestedScene.empty()) {
                        flow.playerRole = (Role)chosen;         // 選んだロールを GameFlow に入れる（次の Scene が読む）
                        flow.requestedScene = ui->nextScene;
                    }
                }
                else if (!ui->buttonText.empty() && !ui->nextScene.empty()) {
                    ImVec2 bs(220.0f, 44.0f);
                    ImGui::SetCursorScreenPos(ImVec2(imgPos.x + (viewWidth - bs.x) * 0.5f, y + 10.0f));
                    ImGui::PushID(slot.get());
                    bool go = ImGui::Button(ui->buttonText.c_str(), bs);
                    ImGui::PopID();
                    if (ImGui::IsKeyPressed(0x0D, false)) go = true;   // Enterでも決定（0x0D = VK_RETURN）
                    if (go && flow.requestedScene.empty()) flow.requestedScene = ui->nextScene;
                }
                break;   // 画面は1つだけ描く
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

        // ---- ライトの view-proj（平行投影）。影が出る範囲を、カメラの前方に追従させる ----
        XMVECTOR ld = XMVector3Normalize(XMVectorSet(lighting.direction.x, lighting.direction.y, lighting.direction.z, 0));
        XMVECTOR up = (fabsf(XMVectorGetY(ld)) > 0.99f) ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);

        // 範囲の中心：カメラの位置から、水平な前方へ少し進めた点（後ろより前を広く取る）
        const float range = (lighting.shadowRange > 1.0f) ? lighting.shadowRange : 1.0f;   // 影が出る正方形の一辺（m）
        XMMATRIX camWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));               // r[3]=カメラ位置, r[2]=前方
        XMVECTOR forward = XMVectorSetY(camWorld.r[2], 0.0f);
        if (XMVectorGetX(XMVector3LengthSq(forward)) > 1e-6f) forward = XMVector3Normalize(forward);
        else forward = XMVectorZero();                                                     // 真上・真下を向いているとき
        XMVECTOR center = XMVectorAdd(camWorld.r[3], XMVectorScale(forward, range * 0.35f));

        // 中心を「シャドウマップ1画素ぶん」の刻みに丸める。
        // 丸めないと、カメラが少し動くたびに影の縁がちらつく（画素の区切りの位置が毎フレームずれるため）。
        XMMATRIX lightRot = XMMatrixLookAtLH(XMVectorZero(), ld, up);                      // 向きだけのライト行列
        const float texel = range / (float)shadowMap.GetSize();
        XMFLOAT3 c; XMStoreFloat3(&c, XMVector3TransformCoord(center, lightRot));          // ライトから見た座標へ
        c.x = floorf(c.x / texel) * texel;
        c.y = floorf(c.y / texel) * texel;
        center = XMVector3TransformCoord(XMLoadFloat3(&c), XMMatrixInverse(nullptr, lightRot));   // ワールドへ戻す

        XMVECTOR lpos = XMVectorSubtract(center, XMVectorScale(ld, 25.0f));                // 中心から光と逆向きに 25m
        XMMATRIX lView = XMMatrixLookAtLH(lpos, center, up);
        XMMATRIX lProj = XMMatrixOrthographicLH(range, range, 0.1f, 60.0f);
        XMFLOAT4X4 lightVP; XMStoreFloat4x4(&lightVP, lView * lProj);

        // シャドウパス
        shadowMap.BeginPass(context, lightVP);
        RenderSystem::RenderShadow(world, shadowMap, *cubeMesh, cache, device, context);
        RenderSystem::RenderShadowSkinned(world, shadowMap, GlobalSkinnedCache(), device, context);
        // 本描画
        rt.Begin(context, 0.10f, 0.12f, 0.15f, 1.0f);
        if (rt.GetWidth() <= 0 || rt.GetHeight() <= 0 || !cubeMesh) return;

        XMFLOAT4X4 viewProj;
        XMStoreFloat4x4(&viewProj, XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));
        XMMATRIX invView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view));
        XMFLOAT3 camPos; XMStoreFloat3(&camPos, invView.r[3]);

        skybox.Render(context, viewProj, camPos, envMap.Sky(), lighting);
        // 空は色だけを描く。ここから先（メッシュ）は、色に加えて法線も書き出す
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
        // シャドウSRVを外す（次フレームのシャドウ書き込みと競合させない）
        ID3D11ShaderResourceView* nul = nullptr;
        context->PSSetShaderResources(1, 1, &nul);

        // ポストプロセス：シーンの画像（rt）を読んで、表示用の画像（finalRT）に書き出す
                // ポストプロセス：シーンの画像（rt の色・法線・深度）を読んで、表示用の画像（finalRT）に書き出す
        postProcess.Apply(context, rt, finalRT, postSettings, proj);
    }

} // namespace uel