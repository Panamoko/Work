#pragma once
#include <cstdint>

namespace uel {

    // 安定ハンドル。generation==0 は無効。破棄後の古いIDは generation 不一致で弾く。
    struct EntityId {
        uint32_t index = 0;
        uint32_t generation = 0;
        bool IsValid() const { return generation != 0; }
        bool operator==(const EntityId& o) const { return index == o.index && generation == o.generation; }
        bool operator!=(const EntityId& o) const { return !(*this == o); }
    };

} // namespace uel