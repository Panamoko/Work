#include "Engine/Scene/World.h"
#include <algorithm>

namespace uel {

    EntityId World::CreateEntity(const std::string& name)
    {
        uint32_t index;
        if (!freeList.empty()) {
            index = freeList.back();
            freeList.pop_back();
        }
        else {
            index = static_cast<uint32_t>(entities.size());
            entities.emplace_back(nullptr);
            generations.push_back(0);
        }

        generations[index]++;                                  // 世代を進める（1以上＝有効）
        if (generations[index] == 0) generations[index] = 1;   // ラップ回避

        auto e = std::make_unique<Entity>();
        e->id.index = index;
        e->id.generation = generations[index];
        e->name = name;
        entities[index] = std::move(e);
        return entities[index]->id;
    }

    void World::DestroyEntity(EntityId id)
    {
        if (!Get(id)) return;
        uint32_t index = id.index;
        entities[index].reset();        // Component も一緒に破棄
        freeList.push_back(index);      // スロット再利用（generation は次Createで+1され古いIDを無効化）
    }

    Entity* World::Get(EntityId id)
    {
        if (!id.IsValid()) return nullptr;
        if (id.index >= entities.size()) return nullptr;
        if (generations[id.index] != id.generation) return nullptr; // 古いID
        return entities[id.index].get();
    }

    Entity* World::RestoreEntity(EntityId id, const std::string& name)
    {
        uint32_t index = id.index;
        if (index >= entities.size()) {
            entities.resize(index + 1);
            generations.resize(index + 1, 0);
        }
        // このindexが空きリストにあれば取り除く
        freeList.erase(std::remove(freeList.begin(), freeList.end(), index), freeList.end());

        generations[index] = id.generation;
        auto e = std::make_unique<Entity>();
        e->id = id;
        e->name = name;
        entities[index] = std::move(e);
        return entities[index].get();
    }

    World World::Clone() const
    {
        World w;
        w.entities.resize(entities.size());
        w.generations = generations;
        w.freeList = freeList;
        for (size_t i = 0; i < entities.size(); ++i) {
            if (!entities[i]) continue;
            auto e = std::make_unique<Entity>();
            e->id = entities[i]->id;
            e->name = entities[i]->name;
            e->parent = entities[i]->parent;
            e->children = entities[i]->children;
            for (const auto& c : entities[i]->components)
                e->components.push_back(c->Clone());
            w.entities[i] = std::move(e);
        }
        return w;
    }


    void World::FlushDestroy()
    {
        if (pendingDestroy.empty()) return;
        std::vector<EntityId> ids;
        ids.swap(pendingDestroy);       // 消している最中に予約が増えても壊れないように、先に取り出す
        for (EntityId id : ids) DestroyEntity(id);     // すでに消えている Entity は、DestroyEntity が何もしない
    }

} // namespace uel