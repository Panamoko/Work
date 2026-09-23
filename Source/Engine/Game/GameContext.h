#pragma once
struct ID3D11Device;
namespace uel {
    class World; class Entity; class MeshCache; struct InputState;
    struct GameContext {
        World& world;
        Entity& self;
        MeshCache& meshCache;
        ID3D11Device* device;
        float         dt;
        const InputState& input;
    };
}