#include "Engine/Components/SpawnerComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Serialization/StageSerializer.h"
#include "Engine/Components/GuideComponent.h"
#include <cmath>
#include <algorithm>

namespace uel {

    float SpawnerComponent::Rand01()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;   // xorshift32（軽い乱数）
        return (rng & 0xFFFFFF) / 16777216.0f;
    }

    // 始めてよいか（5-2）。activateAtStop が 0 なら、すぐ始める（今までどおり）。
    // 1 以上なら、案内ロボットが「着いた止まる場所の数」がその番号に届いたら始める（UE で、トリガーが Spawner を起動する作り）。
    // ロボットの ID は覚えておき、見つからないときだけ 0.5 秒ごとに探し直す（毎フレーム Scene を探さない）
    bool SpawnerComponent::CheckActivation(GameContext& ctx)
    {
        if (activateAtStop <= 0) return true;

        const GuideComponent* guide = nullptr;
        if (Entity* g = ctx.world.Get(guideId)) guide = g->GetComponent<GuideComponent>();
        if (!guide) {
            guideSearchTimer -= ctx.dt;
            if (guideSearchTimer > 0.0f) return false;
            guideSearchTimer = 0.5f;
            for (const auto& slot : ctx.world.Slots()) {
                if (!slot) continue;
                if (const auto* gc = slot->GetComponent<GuideComponent>()) { guide = gc; guideId = slot->id; break; }
            }
            if (!guide) { errorText = "Spawner: no guide robot"; return false; }
        }
        // 道の止まる場所が足りない（その番号に着くことが無い）。道を集めるのはロボットの最初の Update なので、
        // 集める前（止まる場所の数が 0 で、エラーでもない）は判断しない
        if (guide->GetStopCount() > 0 && activateAtStop > guide->GetStopCount()) { errorText = "Spawner: stop not on route"; return false; }
        errorText = nullptr;
        return guide->GetStopsReached() >= activateAtStop;
    }

    void SpawnerComponent::Update(GameContext& ctx)
    {
        if (finished) return;
        float dt = ctx.dt;
        if (!started) {
            if (!CheckActivation(ctx)) return;      // ロボットを待っている（眠っている）
            started = true; state = State::Waiting; timer = firstDelay;
        }
        if (banner > 0.0f) banner -= dt;

        // 倒された（Destroyされた）敵をリストから外す
        alive.erase(std::remove_if(alive.begin(), alive.end(),
            [&](EntityId id) { return ctx.world.Get(id) == nullptr; }), alive.end());

        switch (state) {
        case State::Waiting:                       // 休憩中
            timer -= dt;
            if (timer <= 0.0f) {
                ++wave; spawned = 0; timer = 0.0f;
                banner = 2.0f;
                state = State::Spawning;
            }
            break;

        case State::Spawning:                      // 1体ずつ出す
            timer -= dt;
            if (timer <= 0.0f) {
                auto* st = ctx.self.GetComponent<TransformComponent>();
                EntityId id;
                if (!st || prefabPath.empty() ||
                    !StageSerializer::InstantiatePrefab(ctx.world, prefabPath.c_str(), id)) {
                    failed = true; finished = true;    // Prefabが無い等 → 以後は湧かせない
                    return;
                }
                if (Entity* e = ctx.world.Get(id)) {
                    float ang = Rand01() * 6.2831853f;
                    float dist = sqrtf(Rand01()) * spawnRadius;   // 円内に均等に散らす
                    if (auto* t = e->GetComponent<TransformComponent>())
                        t->position = { st->position.x + cosf(ang) * dist, st->position.y,
                                        st->position.z + sinf(ang) * dist };
                }
                alive.push_back(id);
                ++spawned;
                timer = spawnInterval;
            }
            if (spawned >= WaveSize()) state = State::Fighting;
            break;

        case State::Fighting:                      // 全滅待ち
            if (alive.empty()) {
                if (wave >= waveCount) finished = true;          // 全ウェーブ撃破
                else { state = State::Waiting; timer = waveDelay; }
            }
            break;
        }
    }

} // namespace uel