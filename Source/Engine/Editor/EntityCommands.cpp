#include "Engine/Editor/EntityCommands.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/TransformComponent.h"
#include "Engine/Components/MeshRendererComponent.h"
#include "Engine/Serialization/StageSerializer.h"

#include <memory>
#include <string>
#include <vector>

namespace uel {
    namespace {

        // Entity を丸ごと復元するためのデータ（全コンポーネントの複製を持つ）
        struct EntityRecord {
            EntityId id;
            std::string name;
            std::vector<std::unique_ptr<Component>> comps;
        };

        EntityRecord Capture(const Entity* e) {
            EntityRecord r;
            r.id = e->id; r.name = e->name;
            for (const auto& c : e->components) r.comps.push_back(c->Clone());
            return r;
        }

        void Rebuild(World& world, const EntityRecord& r) {
            Entity* e = world.RestoreEntity(r.id, r.name);
            if (!e) return;
            e->components.clear();
            for (const auto& c : r.comps) e->components.push_back(c->Clone());   // 何度でもUndo/Redoできるよう毎回複製
        }

        class CreateEntityCommand : public Command {
        public:
            CreateEntityCommand(World& w, EntityRecord r) : world(w), rec(std::move(r)) {}
            void Redo() override { Rebuild(world, rec); }
            void Undo() override { world.DestroyEntity(rec.id); }
        private: World& world; EntityRecord rec;
        };

        class DeleteEntityCommand : public Command {
        public:
            DeleteEntityCommand(World& w, EntityRecord r) : world(w), rec(std::move(r)) {}
            void Redo() override { world.DestroyEntity(rec.id); }
            void Undo() override { Rebuild(world, rec); }
        private: World& world; EntityRecord rec;
        };

        class RenameCommand : public Command {
        public:
            RenameCommand(World& w, EntityId i, std::string o, std::string n)
                : world(w), id(i), oldName(std::move(o)), newName(std::move(n)) {
            }
            void Undo() override { if (Entity* e = world.Get(id)) e->name = oldName; }
            void Redo() override { if (Entity* e = world.Get(id)) e->name = newName; }
        private:
            World& world; EntityId id; std::string oldName, newName;
        };

    } // anonymous

    EntityId EditorCreateEntity(World& world, CommandHistory& history, const char* name)
    {
        EntityId id = world.CreateEntity(name);
        Entity* e = world.Get(id);
        e->AddComponent<TransformComponent>();
        e->AddComponent<MeshRendererComponent>(); // 既定：灰色の立方体
        history.Push(std::make_unique<CreateEntityCommand>(world, Capture(e)));
        return id;
    }

    EntityId EditorDuplicateEntity(World& world, CommandHistory& history, EntityId src)
    {
        Entity* s = world.Get(src);
        if (!s) return EntityId{};

        EntityId id = world.CreateEntity(s->name + " Copy");
        Entity* e = world.Get(id);
        for (const auto& c : s->components) e->components.push_back(c->Clone());   // 全コンポーネントを複製
        history.Push(std::make_unique<CreateEntityCommand>(world, Capture(e)));
        return id;
    }

    void EditorDeleteEntity(World& world, CommandHistory& history, EntityId id)
    {
        Entity* e = world.Get(id);
        if (!e) return;
        EntityRecord r = Capture(e);
        world.DestroyEntity(id);
        history.Push(std::make_unique<DeleteEntityCommand>(world, std::move(r)));
    }

    void EditorRenameEntity(World& world, CommandHistory& history, EntityId id,
        const std::string& oldName, const std::string& newName)
    {
        Entity* e = world.Get(id);
        if (!e || newName.empty() || oldName == newName) return;
        e->name = newName;
        history.Push(std::make_unique<RenameCommand>(world, id, oldName, newName));
    }

    EntityId EditorSpawnPrefab(World& world, CommandHistory& history, const std::string& path)
    {
        EntityId id;
        if (!StageSerializer::InstantiatePrefab(world, path.c_str(), id)) return EntityId{};
        if (Entity* e = world.Get(id))
            history.Push(std::make_unique<CreateEntityCommand>(world, Capture(e)));   // Undoで消せる
        return id;
    }

} // namespace uel