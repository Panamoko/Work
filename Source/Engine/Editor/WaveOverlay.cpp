#include "Engine/Editor/WaveOverlay.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/SpawnerComponent.h"
#include "Engine/Components/OpponentComponent.h"
#include "Engine/Components/GuideComponent.h"
#include "Engine/Components/BossComponent.h"
#include "Engine/Components/HPComponent.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace uel {
    namespace {
        // 大きい文字を、黒い影を付けて横の真ん中に描く
        void BigCenterText(ImDrawList* dl, const OverlayView& v, const char* txt, float size, float y, ImU32 col, float alpha)
        {
            ImFont* font = ImGui::GetFont();
            const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, txt);
            const ImVec2 p(v.imgPos.x + (v.width - ts.x) * 0.5f, y);
            dl->AddText(font, size, ImVec2(p.x + 2.0f, p.y + 2.0f), IM_COL32(0, 0, 0, (int)(200 * alpha)), txt);
            dl->AddText(font, size, p, (col & ~IM_COL32_A_MASK) | ((ImU32)(255 * alpha) << IM_COL32_A_SHIFT), txt);
        }

        void ShadowTextAt(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* txt)
        {
            dl->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 200), txt);
            dl->AddText(pos, col, txt);
        }
    } // namespace

    void DrawWaveHud(ImDrawList* dl, World& world, const OverlayView& v)
    {
        // 1回だけ Scene を見て、要る物を集める
        SpawnerComponent* sp = nullptr;          // 最初の Spawner
        SpawnerComponent* errSp = nullptr;       // エラーのある Spawner
        SpawnerComponent* bannerSp = nullptr;    // 「WAVE n」を出している Spawner（残り時間がいちばん長い＝いちばん新しい）
        const GuideComponent* guide = nullptr;
        const Entity* boss = nullptr;            // ボス（最初に見つかった1体）
        int enemies = 0;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            if (auto* s = slot->GetComponent<SpawnerComponent>()) {
                if (!sp) sp = s;
                if (!errSp && s->HasError()) errSp = s;
                if (s->BannerTime() > 0.0f && (!bannerSp || s->BannerTime() > bannerSp->BannerTime())) bannerSp = s;
            }
            if (!guide) guide = slot->GetComponent<GuideComponent>();
            if (!boss && slot->GetComponent<BossComponent>() && slot->GetComponent<HPComponent>()) boss = slot.get();
            if (slot->GetComponent<OpponentComponent>()) ++enemies;
        }

        // ウェーブ表示（Spawner がある時）。
        // 案内ロボット（止まる場所が1つ以上）がいれば、ウェーブは「止まる場所」で数える（5-2）：WAVE 着いた止まる場所の数 / 止まる場所の数。
        // いなければ今までどおり、最初の Spawner のウェーブ。大きな「WAVE n」は、今いちばん新しく始まった Spawner のもの
        const bool byStops = guide && guide->GetStopCount() > 0;
        if (sp) {
            char wbuf[96];
            if (errSp) snprintf(wbuf, sizeof(wbuf), "%s", errSp->ErrorText());
            else if (byStops) snprintf(wbuf, sizeof(wbuf), "WAVE %d / %d    ENEMIES %d", guide->GetStopsReached(), guide->GetStopCount(), enemies);
            else snprintf(wbuf, sizeof(wbuf), "WAVE %d / %d    ENEMIES %d", sp->CurrentWave(), sp->waveCount, enemies);
            const ImVec2 ts = ImGui::CalcTextSize(wbuf);
            dl->AddText(ImVec2(v.imgPos.x + (v.width - ts.x) * 0.5f, v.imgPos.y + 12.0f), errSp ? IM_COL32(255, 120, 120, 255) : IM_COL32(255, 255, 255, 230), wbuf);

            if (bannerSp) {                                    // ウェーブ開始の大きな表示
                const int n = (byStops && bannerSp->activateAtStop > 0) ? bannerSp->activateAtStop : bannerSp->CurrentWave();
                char bbuf[32]; snprintf(bbuf, sizeof(bbuf), "WAVE %d", n);
                const float a = (bannerSp->BannerTime() > 1.0f) ? 1.0f : bannerSp->BannerTime();   // 最後の1秒でフェードアウト
                BigCenterText(dl, v, bbuf, 48.0f, v.imgPos.y + v.height * 0.25f, IM_COL32(255, 220, 80, 255), a);
            }
        }

        // ボスの HP バー（5-3）：WAVE の行の下に、名前と横長のバー。HP の数字はバーの中に。
        // ボスが現れたら（前のフレームと違うボスを見つけたら）、名前を大きく 2.5 秒出す。
        // 覚えておくのは「最後に見たボスの ID と現れた時刻」だけ（Viewport は1つなので、関数の中の static で足りる）
        static EntityId lastBoss;
        static double appearTime = -100.0;
        if (!boss) { lastBoss = EntityId{}; return; }
        const double now = ImGui::GetTime();
        if (boss->id != lastBoss) { lastBoss = boss->id; appearTime = now; }

        const BossComponent* bc = boss->GetComponent<BossComponent>();
        const HPComponent* hp = boss->GetComponent<HPComponent>();
        const char* name = bc->displayName.empty() ? "BOSS" : bc->displayName.c_str();

        const float bw = (std::min)(560.0f, v.width * 0.5f), bh = 16.0f;
        const float bx = v.imgPos.x + (v.width - bw) * 0.5f;
        const float ny = v.imgPos.y + 38.0f;                       // 名前の上端
        const ImVec2 ns = ImGui::CalcTextSize(name);
        ShadowTextAt(dl, ImVec2(v.imgPos.x + (v.width - ns.x) * 0.5f, ny), IM_COL32(255, 200, 120, 255), name);
        const float by = ny + ns.y + 4.0f;                         // バーの上端
        float frac = (hp->maxHp > 0.0f) ? hp->hp / hp->maxHp : 0.0f;
        frac = (std::max)(0.0f, (std::min)(1.0f, frac));
        dl->AddRectFilled(ImVec2(bx - 2.0f, by - 2.0f), ImVec2(bx + bw + 2.0f, by + bh + 2.0f), IM_COL32(0, 0, 0, 170));
        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(50, 20, 20, 220));
        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw * frac, by + bh), IM_COL32(230, 70, 40, 240));
        dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(255, 255, 255, 160));
        char hbuf[32]; snprintf(hbuf, sizeof(hbuf), "%.0f / %.0f", (std::max)(0.0f, hp->hp), hp->maxHp);
        const ImVec2 hs = ImGui::CalcTextSize(hbuf);
        ShadowTextAt(dl, ImVec2(v.imgPos.x + (v.width - hs.x) * 0.5f, by + (bh - hs.y) * 0.5f), IM_COL32(255, 255, 255, 240), hbuf);

        const double shown = now - appearTime;
        if (shown < 2.5) {                                         // 現れたときの大きな名前（最後の 1 秒で消える）
            const float a = (shown > 1.5) ? (float)(2.5 - shown) : 1.0f;
            BigCenterText(dl, v, name, 56.0f, v.imgPos.y + v.height * 0.25f + 56.0f, IM_COL32(255, 110, 70, 255), a);
        }
    }

} // namespace uel
