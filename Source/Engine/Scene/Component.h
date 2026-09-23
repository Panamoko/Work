#pragma once
namespace uel {
    struct GameContext;

    // データ基底＋ゲームTick（既定は何もしない。UE の ActorComponent 相当）
    class Component {
    public:
        virtual ~Component() = default;
        virtual void Update(GameContext&) {}
    };
}