#include "Engine/Editor/ScreenOverlay.h"
#include "Engine/Components/ScreenUIComponent.h"
#include "Engine/Game/GameFlow.h"
#include "Engine/Game/Role.h"
#include "Engine/Scene/World.h"
#include <imgui.h>
#include <cfloat>
#include <cstdio>
#include <string>

namespace uel {
    namespace {

        // 横の真ん中に文字を書く（大きさを指定できるように、ImGui のフォントで直接描く）。
        // 後ろの Scene の物と重なっても読めるように、黒い影を先に描く（HUD の shadowText と同じ考え方）
        void CenterText(ImDrawList* dl, ImFont* font, const OverlayView& v, const char* txt, float size, float y, ImU32 col)
        {
            const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, txt);
            const ImVec2 pos(v.imgPos.x + (v.width - ts.x) * 0.5f, y);
            dl->AddText(font, size, ImVec2(pos.x + 2.0f, pos.y + 2.0f), IM_COL32(0, 0, 0, 190), txt);
            dl->AddText(font, size, pos, col, txt);
        }

        // ロールの色（AI 仲間の名札と同じ。Tank 青・DPS 橙・Support 緑）
        ImU32 RoleColor(Role r)
        {
            switch (r) {
            case Role::Tank:    return IM_COL32(110, 170, 255, 255);
            case Role::DPS:     return IM_COL32(255, 150, 90, 255);
            case Role::Support: return IM_COL32(120, 230, 140, 255);
            }
            return IM_COL32(255, 255, 255, 255);
        }

        // Result の中身（5-5a）。y から下へ書き、次に書く高さを返す。
        // 値は GameRule が決着したときに GameFlow に書いたもの（ここでは World を探さない）
        float DrawResult(ImDrawList* dl, ImFont* font, const OverlayView& v, const GameFlow& flow, float y)
        {
            const bool clear = (flow.lastResult == "CLEAR");
            CenterText(dl, font, v, flow.lastResult.c_str(), 40.0f, y, clear ? IM_COL32(120, 230, 120, 255) : IM_COL32(255, 90, 90, 255));
            y += 50.0f;

            if (!clear) {       // ゲームオーバーの理由
                const char* why = nullptr;
                if (flow.lastLoseReason == LoseReason::PlayerDead)     why = u8"プレイヤーが倒れた";
                else if (flow.lastLoseReason == LoseReason::PartyDown) why = u8"3人とも戦えなくなった";
                if (why) { CenterText(dl, font, v, why, 22.0f, y, IM_COL32(255, 160, 150, 255)); y += 36.0f; }
            }

            char buf[96];
            const float rowSize = 24.0f, rowStep = 32.0f;
            const ImU32 white = IM_COL32(230, 230, 230, 255);

            snprintf(buf, sizeof(buf), u8"ロール  %s", RoleName(flow.lastRole));
            CenterText(dl, font, v, buf, rowSize, y, RoleColor(flow.lastRole)); y += rowStep;

            const int   minutes = (int)(flow.lastTime / 60.0f);
            const float seconds = flow.lastTime - minutes * 60.0f;
            snprintf(buf, sizeof(buf), u8"タイム  %d:%04.1f", minutes, seconds);
            CenterText(dl, font, v, buf, rowSize, y, white); y += rowStep;

            if (flow.lastStopCount > 0) {     // 案内ロボットのいる Stage だけ（WAVE の数え方は HUD と同じ＝着いた止まる場所の数）
                snprintf(buf, sizeof(buf), u8"到達  WAVE %d / %d%s", flow.lastStopsReached, flow.lastStopCount,
                    flow.lastBossDefeated ? u8"   ボス撃破" : "");
                CenterText(dl, font, v, buf, rowSize, y, flow.lastBossDefeated ? IM_COL32(255, 215, 90, 255) : white);
                y += rowStep;
            }
            return y + 4.0f;
        }

    } // namespace

    void DrawScreenUI(ImDrawList* dl, World& world, GameFlow& flow, const OverlayView& v)
    {
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            auto* ui = slot->GetComponent<ScreenUIComponent>();
            if (!ui) continue;

            const ImVec2 imgPos = v.imgPos;
            const float viewWidth = v.width, viewHeight = v.height;
            dl->AddRectFilled(imgPos, ImVec2(imgPos.x + viewWidth, imgPos.y + viewHeight), IM_COL32(0, 0, 0, 110));
            ImFont* font = ImGui::GetFont();

            float y = imgPos.y + viewHeight * 0.28f;
            if (ui->showResult && !flow.lastResult.empty()) y = imgPos.y + viewHeight * 0.18f;   // Result は行が多いので少し上から
            CenterText(dl, font, v, ui->title.c_str(), 56.0f, y, IM_COL32(255, 255, 255, 255)); y += 72.0f;
            if (ui->showResult && !flow.lastResult.empty()) y = DrawResult(dl, font, v, flow, y);
            if (!ui->subtitle.empty()) { CenterText(dl, font, v, ui->subtitle.c_str(), 22.0f, y, IM_COL32(200, 200, 200, 255)); y += 40.0f; }

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

} // namespace uel
