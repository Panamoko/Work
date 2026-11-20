#include "Engine/Components/GuideComponent.h"
#include "Engine/Components/GuidePointComponent.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Components/PlayerComponent.h"
#include "Engine/Components/HPComponent.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Components/SpawnerComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Game/InputState.h"
#include "Engine/Scene/World.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Physics/GroundQuery.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace uel {
    namespace {

        constexpr float kArriveDistance = 0.05f;    // 経由点にこれ（m）より近づいたら着いたとみなす
        constexpr float kTurnSpeed = 360.0f;        // 向きを変える速さ（度/秒）。急に向きが変わらないように
        constexpr float kGroundInterval = 0.1f;     // 進んでいる間、地面の高さを調べる間隔（秒）。毎フレームは調べない
        constexpr float kGroundFollow = 12.0f;      // 地面の高さへ寄せる速さ（大きいほどすぐ合わせる）
        constexpr float kWaveCheckInterval = 0.2f;  // 止まっている間、ウェーブが終わったかを調べる間隔（秒。5-2）
        constexpr float kClearBannerTime = 3.0f;    // 「WAVE CLEAR」を出す時間（秒。5-2）

        float DistXZ(const XMFLOAT3& a, const XMFLOAT3& b)
        {
            const float dx = a.x - b.x, dz = a.z - b.z;
            return std::sqrt(dx * dx + dz * dz);
        }

        // 今の向き（度）を、目標の向き（度）へ、最大 maxStep 度だけ回す（近いほうの回り方で）
        float TurnToward(float current, float target, float maxStep)
        {
            float diff = std::fmod(target - current + 540.0f, 360.0f) - 180.0f;    // -180〜180 にそろえる
            if (diff > maxStep) diff = maxStep;
            else if (diff < -maxStep) diff = -maxStep;
            return current + diff;
        }

        // 足元の地面の高さ。AllyComponent・OpponentComponent と同じ調べ方（表示中の MeshRenderer のメッシュを真下に調べる）。
        // 今の位置より 1.5m 以上高い面は、頭の上の物とみなして無視する
        bool FindGround(GameContext& ctx, const Entity& self, const XMFLOAT3& p, float& outY)
        {
            bool  found = false;
            float best = -1e9f;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot || slot.get() == &self) continue;
                auto* gm = slot->GetComponent<MeshRendererComponent>();
                if (!gm || !gm->visible) continue;
                auto* gt = slot->GetComponent<TransformComponent>();
                if (!gt) continue;
                auto mp = gm->assetId.empty() ? ctx.meshCache.GetCube(ctx.device)
                    : ctx.meshCache.GetOrLoad(ctx.device, gm->assetId);
                if (!mp || !mp->IsValid()) continue;
                XMFLOAT4X4 wm = gt->GetWorldMatrix();
                float y;
                if (RaycastGroundColumn(*mp, XMLoadFloat4x4(&wm), p.x, p.z, y, p.y + 1.5f, &gm->noCollisionSubmesh) && y > best) {
                    best = y;
                    found = true;
                }
            }
            if (found) outY = best;
            return found;
        }
    } // namespace

    // 経由点を集めて、order の小さい順に並べる（同じ order は Scene に置いた順。stable_sort で順番を保つ）
    void GuideComponent::BuildRoute(World& world)
    {
        struct Item { int order; RoutePoint p; };
        std::vector<Item> items;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            const auto* gp = slot->GetComponent<GuidePointComponent>();
            const auto* t = slot->GetComponent<TransformComponent>();
            if (!gp || !t) continue;
            items.push_back({ gp->order, { t->position, gp->stopHere } });
        }
        std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.order < b.order; });

        route.clear();
        route.reserve(items.size());
        stopCount = 0;
        for (const auto& it : items) {
            route.push_back(it.p);
            if (it.p.stop) ++stopCount;
        }
        routeEmpty = route.empty();
        next = 0;
        stopsReached = 0;
        state = routeEmpty ? State::Finished : State::Waiting;
    }

    // 今止まっている場所（stopsReached 番目）の Spawner が、1つでもまだ終わっていなければ true。
    // まだ始まっていない（眠っている）Spawner も「終わっていない」に入る（着いたフレームの順番に左右されない）
    bool GuideComponent::StopWaveRemaining(World& world) const
    {
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            const auto* sp = slot->GetComponent<SpawnerComponent>();
            if (sp && sp->activateAtStop == stopsReached && !sp->IsFinished()) return true;
        }
        return false;
    }

    void GuideComponent::Update(GameContext& ctx)
    {
        Entity& self = ctx.self;
        auto* t = self.GetComponent<TransformComponent>();
        if (!t) return;
        const float dt = ctx.dt;

        if (!initialized) {
            BuildRoute(ctx.world);
            groundY = t->position.y;
            initialized = true;
        }

        // プレイヤー（覚えている ID が無効になったときだけ探し直す）
        Entity* player = ctx.world.Get(playerId);
        if (!player || !player->GetComponent<PlayerComponent>()) {
            player = nullptr;
            playerId = EntityId{};
            for (const auto& slot : ctx.world.Slots())
                if (slot && slot->GetComponent<PlayerComponent>()) { player = slot.get(); playerId = slot->id; break; }
        }

        canSignal = false;
        float speedNow = 0.0f;

        // 止まる場所のウェーブ（5-2）：止まっている間、その場所の Spawner が全部終わったかを 0.2 秒ごとに調べる。
        // 終わるまでは合図を受け付けない。終わった瞬間に「WAVE CLEAR」を出す（最後の点では出さない。ステージクリアになるため）
        if (clearBanner > 0.0f) clearBanner -= dt;
        if (state != State::Moving && stopsReached > 0) {
            waveCheckTimer -= dt;
            if (waveCheckTimer <= 0.0f) {
                waveCheckTimer = kWaveCheckInterval;
                const bool wasFighting = fighting;
                fighting = StopWaveRemaining(ctx.world);
                if (wasFighting && !fighting && next < route.size()) clearBanner = kClearBannerTime;
            }
        }
        else {
            fighting = false;
        }

        if (state == State::Waiting && !fighting) {
            // 合図を待つ：プレイヤーが近く（水平に signalRange 以内）にいて、戦える（HP が残っている。ダウン中は合図できない）なら、
            // F で出発する。近くにいる間は、プレイヤーのほうを向く（話しかけられる相手だと分かるように）
            const TransformComponent* pt = player ? player->GetComponent<TransformComponent>() : nullptr;
            const HPComponent* php = player ? player->GetComponent<HPComponent>() : nullptr;
            const bool alive = !php || php->hp > 0.0f;
            if (pt && alive && next < route.size() && DistXZ(pt->position, t->position) <= signalRange) {
                canSignal = true;
                const float fx = pt->position.x - t->position.x, fz = pt->position.z - t->position.z;
                if (fx * fx + fz * fz > 0.01f)
                    t->rotationEuler.y = TurnToward(t->rotationEuler.y, std::atan2(fx, fz) * 57.2958f, kTurnSpeed * dt);
                if (ctx.input.signalPressed) {
                    state = State::Moving;
                    canSignal = false;
                    groundTimer = 0.0f;     // 出発した最初のフレームに地面を調べる
                }
            }
        }

        if (state == State::Moving && next < route.size()) {
            // 次の経由点へまっすぐ進む（水平だけ。高さは下で地面に合わせる）
            const RoutePoint& goal = route[next];
            const float dx = goal.pos.x - t->position.x, dz = goal.pos.z - t->position.z;
            const float len = std::sqrt(dx * dx + dz * dz);
            const float step = moveSpeed * dt;
            if (len <= (std::max)(step, kArriveDistance)) {
                // 着いた：点の上にそろえる。止まる場所なら止まって合図を待ち、最後の点なら終わり
                t->position.x = goal.pos.x;
                t->position.z = goal.pos.z;
                if (dt > 0.0f) speedNow = len / dt;
                ++next;
                if (goal.stop) {
                    ++stopsReached;
                    state = (next < route.size()) ? State::Waiting : State::Finished;
                    fighting = StopWaveRemaining(ctx.world);    // 着いたらすぐ調べる（ウェーブがあれば、その間は合図を受け付けない）
                    waveCheckTimer = kWaveCheckInterval;
                }
                else if (next >= route.size()) {
                    state = State::Finished;
                }
            }
            else {
                t->position.x += dx / len * step;
                t->position.z += dz / len * step;
                t->rotationEuler.y = TurnToward(t->rotationEuler.y, std::atan2(dx, dz) * 57.2958f, kTurnSpeed * dt);
                speedNow = moveSpeed;
            }

            // 地面の高さ：0.1 秒ごとに調べ、その高さへなめらかに寄せる（全部のメッシュを調べるので、毎フレームは調べない）。
            // 地面が見つからない所（メッシュの外）では、向かっている経由点の高さへ寄せる
            groundTimer -= dt;
            if (groundTimer <= 0.0f) {
                groundTimer = kGroundInterval;
                float gy;
                groundY = FindGround(ctx, self, t->position, gy) ? gy : goal.pos.y;
            }
            t->position.y += (groundY - t->position.y) * (std::min)(1.0f, kGroundFollow * dt);
        }

        // アニメのパラメータ（変わったときだけ書く）
        if (auto* an = self.GetComponent<AnimatorComponent>()) {
            if (std::fabs(speedNow - speedAnim) > 0.01f) {
                speedAnim = speedNow;
                an->params.SetIfExists(kAnimSpeed, speedNow);
            }
            const int waitingNow = (state == State::Waiting) ? 1 : 0;
            if (waitingNow != waitingAnim) {
                waitingAnim = waitingNow;
                an->params.SetIfExists(kAnimWaiting, waitingNow ? 1.0f : 0.0f);
            }
        }
    }

} // namespace uel
