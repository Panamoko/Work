#pragma once
#include "Engine/Editor/CommandHistory.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/EntityId.h"
#include <memory>
#include <cstring>
#include <DirectXMath.h>

namespace uel {

    struct TransformSnapshot {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 rotationEuler;
        DirectX::XMFLOAT3 scale;
    };

    class TransformEditCommand : public Command {
    public:
        TransformEditCommand(World& w, EntityId id, const TransformSnapshot& b, const TransformSnapshot& a)
            : world(w), id(id), before(b), after(a) {
        }
        void Undo() override { Apply(before); }
        void Redo() override { Apply(after); }
    private:
        void Apply(const TransformSnapshot& s) {
            if (Entity* e = world.Get(id))
                if (auto* t = e->GetComponent<TransformComponent>()) {
                    t->position = s.position; t->rotationEuler = s.rotationEuler; t->scale = s.scale;
                }
        }
        World& world; EntityId id; TransformSnapshot before, after;
    };

    // 1ドラッグ＝1コマンド のための「編集セッション」
    struct TransformEditSession {
        bool active = false;
        EntityId entity;
        TransformSnapshot before{};
    };

    inline TransformSnapshot SnapshotOf(const TransformComponent& t) {
        return { t.position, t.rotationEuler, t.scale };
    }
    inline void BeginTransformEdit(TransformEditSession& s, EntityId id, const TransformComponent& t) {
        if (!s.active) { s.active = true; s.entity = id; s.before = SnapshotOf(t); }
    }
    inline void CommitTransformEdit(TransformEditSession& s, World& world, CommandHistory& history) {
        if (!s.active) return;
        s.active = false;
        Entity* e = world.Get(s.entity); if (!e) return;
        auto* t = e->GetComponent<TransformComponent>(); if (!t) return;
        TransformSnapshot after = SnapshotOf(*t);
        if (std::memcmp(&after, &s.before, sizeof(TransformSnapshot)) != 0)   // 変化があった時だけ
            history.Push(std::make_unique<TransformEditCommand>(world, s.entity, s.before, after));
    }

} // namespace uel