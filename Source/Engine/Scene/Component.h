#pragma once
#include <memory>

namespace uel {
    struct GameContext;

    class Component {
    public:
        virtual ~Component() = default;
        virtual void Update(GameContext&) {}
        virtual std::unique_ptr<Component> Clone() const = 0;   // ★複製用（全Componentが実装）
    };
}