#pragma once
struct ID3D11Device;
namespace uel {
    class World; class Entity; class MeshCache; struct InputState; class EngineNavGrid;
    struct GameContext {
        World& world;
        Entity& self;
        MeshCache& meshCache;
        ID3D11Device* device;
        float         dt;
        const InputState& input;
        EngineNavGrid* nav = nullptr;   // ★経路探索用（無ければ直行）
    };
}