#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Engine/Scene/Entity.h"

namespace uel {

    // Entity Registry と Component Storage を所有する（仕様書17章）。
    class World {
    public:
        EntityId CreateEntity(const std::string& name);
        void     DestroyEntity(EntityId id);
        Entity* Get(EntityId id);   // 無効/古いIDなら nullptr
        void Clear() { entities.clear(); generations.clear(); freeList.clear(); pendingDestroy.clear(); }

        // 「後で消す」予約。Component の Update の中から自分自身の Entity を消したいときに使う。
        // Update の途中で自分を消すと、いま実行中の Component がその場で無くなってしまうので、
        // 予約だけしておき、全員の Update が終わった後に FlushDestroy() でまとめて消す。
        void DestroyLater(EntityId id) { pendingDestroy.push_back(id); }
        void FlushDestroy();

        // 構造変更のUndo用：指定IDでEntityを復元/確保する低レベルAPI
        Entity* RestoreEntity(EntityId id, const std::string& name);

        // 全スロット（nullptr=空き）。UI/System はこれで反復する。
        const std::vector<std::unique_ptr<Entity>>& Slots() const { return entities; }
        World Clone() const;

    private:
        std::vector<std::unique_ptr<Entity>> entities;    // index = EntityId.index
        std::vector<uint32_t>                generations; // 各スロットの世代
        std::vector<uint32_t>                freeList;    // 再利用可能 index
        std::vector<EntityId>                pendingDestroy;  // 「後で消す」予約（Clone では写さない）
    };

} // namespace uel