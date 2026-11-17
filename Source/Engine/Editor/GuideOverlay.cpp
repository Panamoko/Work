#include "Engine/Editor/GuideOverlay.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/GuideComponent.h"
#include "Engine/Components/GuidePointComponent.h"
#include "Engine/Components/PlayerComponent.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace DirectX;

namespace uel {
    namespace {

        const ImU32 kRouteCol = IM_COL32(80, 220, 255, 230);        // 道の色（水色。敵の経路の黄色・回復の緑と見分ける）
        const ImU32 kEdgeCol = IM_COL32(0, 0, 0, 150);              // 線と文字の黒い縁
        const ImU32 kSelectCol = IM_COL32(255, 220, 60, 255);       // 選んでいる経由点

        // 3D の点を画面へ。カメラの後ろなら false。vp はビュー × 射影（呼ぶ側で1回だけ作って渡す）
        bool Project(const OverlayView& v, FXMMATRIX vp, const XMFLOAT3& p, ImVec2& out)
        {
            const XMVECTOR c = XMVector4Transform(XMVectorSet(p.x, p.y, p.z, 1.0f), vp);
            const float w = XMVectorGetW(c);
            if (w <= 0.0001f) return false;
            out.x = v.imgPos.x + (XMVectorGetX(c) / w * 0.5f + 0.5f) * v.width;
            out.y = v.imgPos.y + (1.0f - (XMVectorGetY(c) / w * 0.5f + 0.5f)) * v.height;
            return true;
        }

        // 3D の線を画面へ。片方の端がカメラの後ろなら、カメラのすぐ前の面で切ってから写す（ViewportPanel の projectSegment と同じ）
        bool ProjectSegment(const OverlayView& v, FXMMATRIX vp, const XMFLOAT3& a, const XMFLOAT3& b, ImVec2& sa, ImVec2& sb)
        {
            const float kNearW = 0.1f;
            XMVECTOR ca = XMVector4Transform(XMVectorSet(a.x, a.y, a.z, 1.0f), vp);
            XMVECTOR cb = XMVector4Transform(XMVectorSet(b.x, b.y, b.z, 1.0f), vp);
            const float wa = XMVectorGetW(ca), wb = XMVectorGetW(cb);
            if (wa < kNearW && wb < kNearW) return false;
            if (wa < kNearW) ca = XMVectorLerp(ca, cb, (kNearW - wa) / (wb - wa));
            else if (wb < kNearW) cb = XMVectorLerp(cb, ca, (kNearW - wb) / (wa - wb));
            auto toScreen = [&](FXMVECTOR c) {
                const float w = XMVectorGetW(c);
                return ImVec2(v.imgPos.x + (XMVectorGetX(c) / w * 0.5f + 0.5f) * v.width,
                    v.imgPos.y + (1.0f - (XMVectorGetY(c) / w * 0.5f + 0.5f)) * v.height);
                };
            sa = toScreen(ca);
            sb = toScreen(cb);
            return true;
        }

        void ShadowText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* txt)
        {
            dl->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 200), txt);
            dl->AddText(pos, col, txt);
        }

        // 縁取りした線
        void EdgedLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float width)
        {
            dl->AddLine(a, b, kEdgeCol, width + 3.0f);
            dl->AddLine(a, b, col, width);
        }
    } // namespace

    void DrawGuideRoute(ImDrawList* dl, World& world, const OverlayView& v, EntityId selected)
    {
        // 経由点を集めて順番に並べる（GuideComponent::BuildRoute と同じ並べ方。stable_sort で同じ番号は置いた順）。
        // 入れ物は使い回す（編集中は毎フレーム呼ばれるので、毎回メモリを確保しない）
        struct Pt { int order; XMFLOAT3 pos; bool stop; bool selected; };
        static std::vector<Pt> pts;
        pts.clear();
        const TransformComponent* robotT = nullptr;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            const auto* t = slot->GetComponent<TransformComponent>();
            if (!t) continue;
            if (const auto* gp = slot->GetComponent<GuidePointComponent>())
                pts.push_back({ gp->order, t->position, gp->stopHere, slot->id == selected });
            if (!robotT && slot->GetComponent<GuideComponent>()) robotT = t;
        }
        if (pts.empty()) return;
        std::stable_sort(pts.begin(), pts.end(), [](const Pt& a, const Pt& b) { return a.order < b.order; });

        const XMMATRIX vp = XMLoadFloat4x4(&v.view) * XMLoadFloat4x4(&v.proj);
        const float lift = 0.1f;   // 地面に埋もれないように少し浮かせる
        auto up = [&](XMFLOAT3 p) { p.y += lift; return p; };

        // 線：ロボット → 最初の点（細く）、点 → 点
        ImVec2 sa, sb;
        if (robotT && ProjectSegment(v, vp, up(robotT->position), up(pts[0].pos), sa, sb))
            EdgedLine(dl, sa, sb, IM_COL32(80, 220, 255, 140), 1.5f);
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            if (ProjectSegment(v, vp, up(pts[i].pos), up(pts[i + 1].pos), sa, sb))
                EdgedLine(dl, sa, sb, kRouteCol, 2.5f);

        // 点と番号。止まる場所は大きい輪と「STOP n」（n は止まる場所の何番目か）
        int stopNo = 0;
        for (const Pt& p : pts) {
            if (p.stop) ++stopNo;
            ImVec2 s;
            if (!Project(v, vp, up(p.pos), s)) continue;
            const ImU32 col = p.selected ? kSelectCol : kRouteCol;
            if (p.stop) {
                dl->AddCircleFilled(s, 9.0f, IM_COL32(0, 0, 0, 120), 20);
                dl->AddCircle(s, 9.0f, kEdgeCol, 20, 5.0f);
                dl->AddCircle(s, 9.0f, col, 20, 2.5f);
            }
            else {
                dl->AddCircleFilled(s, 5.0f, kEdgeCol, 12);
                dl->AddCircleFilled(s, 3.5f, col, 12);
            }
            char label[32];
            if (p.stop) snprintf(label, sizeof(label), "%d  STOP %d", p.order, stopNo);
            else        snprintf(label, sizeof(label), "%d", p.order);
            const ImVec2 ts = ImGui::CalcTextSize(label);
            ShadowText(dl, ImVec2(s.x - ts.x * 0.5f, s.y - ts.y - 12.0f), col, label);
        }
    }

    void DrawGuideHud(ImDrawList* dl, World& world, const Entity& player, const OverlayView& v, bool mouseCaptured)
    {
        // ロボット（最初に見つかった1体だけ）
        const GuideComponent* guide = nullptr;
        const TransformComponent* gt = nullptr;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            if (const auto* g = slot->GetComponent<GuideComponent>()) {
                guide = g;
                gt = slot->GetComponent<TransformComponent>();
                break;
            }
        }
        if (!guide || !gt) return;

        if (guide->HasError()) {    // 経由点が無い（左上の ROLE の下、PartySetup のエラーのさらに下）
            ShadowText(dl, ImVec2(v.imgPos.x + 20.0f, v.imgPos.y + 56.0f), IM_COL32(255, 120, 120, 255), "Guide: no route points");
            return;
        }

        const TransformComponent* myT = player.GetComponent<TransformComponent>();
        const GuideComponent::State state = guide->GetState();

        // 印：ロボットの頭の上に水色のひし形。画面の外・カメラの後ろなら、その方向の画面の端（ダウンした味方の印と同じ出し方）
        const XMFLOAT3 wp{ gt->position.x, gt->position.y + guide->markerHeight * gt->scale.y, gt->position.z };
        const float inset = 48.0f;
        const ImVec2 rectMin(v.imgPos.x + inset, v.imgPos.y + inset);
        const ImVec2 rectMax(v.imgPos.x + v.width - inset, v.imgPos.y + v.height - 130.0f);    // 下は HP・弾数の欄に重ならないように
        XMFLOAT3 vp3;
        XMStoreFloat3(&vp3, XMVector3TransformCoord(XMLoadFloat3(&wp), XMLoadFloat4x4(&v.view)));
        ImVec2 s;
        const bool onScreen = vp3.z > 0.1f && Project(v, XMLoadFloat4x4(&v.view) * XMLoadFloat4x4(&v.proj), wp, s)
            && s.x >= rectMin.x && s.x <= rectMax.x && s.y >= rectMin.y && s.y <= rectMax.y;
        if (!onScreen) {
            float dx = vp3.x, dy = -vp3.y;
            if (dx * dx + dy * dy < 1e-6f) { dx = 0.0f; dy = 1.0f; }
            const float hw = (rectMax.x - rectMin.x) * 0.5f, hh = (rectMax.y - rectMin.y) * 0.5f;
            const float k = (std::min)(std::fabs(dx) > 1e-6f ? hw / std::fabs(dx) : 1e9f,
                std::fabs(dy) > 1e-6f ? hh / std::fabs(dy) : 1e9f);
            s = ImVec2((rectMin.x + rectMax.x) * 0.5f + dx * k, (rectMin.y + rectMax.y) * 0.5f + dy * k);
        }

        // 合図を待っている間は、ひし形を脈打たせる（「こちらの番」だと分かるように）
        const float pulse = (state == GuideComponent::State::Waiting) ? 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 5.0f) : 1.0f;
        const float r = 9.0f + 2.0f * pulse;
        const ImU32 fill = IM_COL32(80, 220, 255, (int)(170 + 85 * pulse));
        dl->AddQuadFilled(ImVec2(s.x, s.y - r - 2.0f), ImVec2(s.x + r + 2.0f, s.y), ImVec2(s.x, s.y + r + 2.0f), ImVec2(s.x - r - 2.0f, s.y), IM_COL32(0, 0, 0, 170));
        dl->AddQuadFilled(ImVec2(s.x, s.y - r), ImVec2(s.x + r, s.y), ImVec2(s.x, s.y + r), ImVec2(s.x - r, s.y), fill);

        // 印の下に、状態と距離
        const float dist = myT ? std::sqrt((gt->position.x - myT->position.x) * (gt->position.x - myT->position.x)
            + (gt->position.z - myT->position.z) * (gt->position.z - myT->position.z)) : 0.0f;
        char txt[64];
        if (state == GuideComponent::State::Waiting)     snprintf(txt, sizeof(txt), u8"合図待ち  %.0fm", dist);
        else if (state == GuideComponent::State::Moving) snprintf(txt, sizeof(txt), u8"移動中  %.0fm", dist);
        else                                             snprintf(txt, sizeof(txt), "%.0fm", dist);
        const ImVec2 ts = ImGui::CalcTextSize(txt);
        float tx = s.x - ts.x * 0.5f;                                           // 文字が画面の端で切れないように
        tx = (std::max)(v.imgPos.x + 6.0f, (std::min)(tx, v.imgPos.x + v.width - ts.x - 6.0f));
        ShadowText(dl, ImVec2(tx, s.y + r + 4.0f), IM_COL32(150, 235, 255, 255), txt);

        // 合図できる：照準の下に「F：ロボットを進める」。「E 長押し：仲間を起こす」が出ているときは、その下に出す。
        // 仲間を起こしている間は出さない（蘇生のバーと重なるため）
        const PlayerComponent* pc = player.GetComponent<PlayerComponent>();
        if (mouseCaptured && guide->CanSignal() && !(pc && pc->IsReviving())) {
            const ImVec2 c(v.imgPos.x + v.width * 0.5f, v.imgPos.y + v.height * 0.5f);
            const float y = c.y + ((pc && pc->CanRevive()) ? 62.0f : 40.0f);
            const char* hint = u8"F：ロボットを進める";
            const ImVec2 hs = ImGui::CalcTextSize(hint);
            ShadowText(dl, ImVec2(c.x - hs.x * 0.5f, y), IM_COL32(150, 235, 255, 255), hint);
        }
    }

} // namespace uel
