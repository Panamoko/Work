#include "Engine/Game/SpawnerComponent.h"
#include "Engine/Game/GameContext.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Serialization/StageSerializer.h"
#include <cmath>
#include <algorithm>

namespace uel {

    float SpawnerComponent::Rand01()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;   // xorshift32iŒy‚¢—”j
        return (rng & 0xFFFFFF) / 16777216.0f;
    }

    void SpawnerComponent::Update(GameContext& ctx)
    {
        if (finished) return;
        float dt = ctx.dt;
        if (!started) { started = true; state = State::Waiting; timer = firstDelay; }
        if (banner > 0.0f) banner -= dt;

        // “|‚³‚ê‚½iDestroy‚³‚ê‚½j“G‚ğƒŠƒXƒg‚©‚çŠO‚·
        alive.erase(std::remove_if(alive.begin(), alive.end(),
            [&](EntityId id) { return ctx.world.Get(id) == nullptr; }), alive.end());

        switch (state) {
        case State::Waiting:                       // ‹xŒe’†
            timer -= dt;
            if (timer <= 0.0f) {
                ++wave; spawned = 0; timer = 0.0f;
                banner = 2.0f;
                state = State::Spawning;
            }
            break;

        case State::Spawning:                      // 1‘Ì‚¸‚Âo‚·
            timer -= dt;
            if (timer <= 0.0f) {
                auto* st = ctx.self.GetComponent<TransformComponent>();
                EntityId id;
                if (!st || prefabPath.empty() ||
                    !StageSerializer::InstantiatePrefab(ctx.world, prefabPath.c_str(), id)) {
                    failed = true; finished = true;    // Prefab‚ª–³‚¢“™ ¨ ˆÈŒã‚Í—N‚©‚¹‚È‚¢
                    return;
                }
                if (Entity* e = ctx.world.Get(id)) {
                    float ang = Rand01() * 6.2831853f;
                    float dist = sqrtf(Rand01()) * spawnRadius;   // ‰~“à‚É‹Ï“™‚ÉU‚ç‚·
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

        case State::Fighting:                      // ‘S–Å‘Ò‚¿
            if (alive.empty()) {
                if (wave >= waveCount) finished = true;          // ‘SƒEƒF[ƒuŒ‚”j
                else { state = State::Waiting; timer = waveDelay; }
            }
            break;
        }
    }

} // namespace uel