#include "Engine/Editor/EntityCommands.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include <memory>
#include <string>
#include <DirectXMath.h>

namespace uel {
    namespace {

        // Entity 全体を復元するのに必要なデータ
        struct EntityRecord {
            EntityId id;
            std::string name;
            bool hasTransform = false;
            DirectX::XMFLOAT3 pos{ 0,0,0 }, rot{ 0,0,0 }, scale{ 1,1,1 };
            bool hasMesh = false;
            std::string meshAssetId;
            DirectX::XMFLOAT4 meshColor{ 0.8f,0.8f,0.8f,1 };
            bool meshVisible = true;
        };

        EntityRecord Capture(Entity* e) {
            EntityRecord r;
            r.id = e->id; r.name = e->name;
            if (auto* t = e->GetComponent<TransformComponent>()) {
                r.hasTransform = true; r.pos = t->position; r.rot = t->rotationEuler; r.scale = t->scale;
            }
            if (auto* m = e->GetComponent<MeshRendererComponent>()) {
                r.hasMesh = true; r.meshAssetId = m->assetId; r.meshColor = m->color; r.meshVisible = m->visible;
            }
            return r;
        }

        void Rebuild(World& world, const EntityRecord& r) {
            Entity* e = world.RestoreEntity(r.id, r.name);
            if (!e) return;
            if (r.hasTransform) { auto* t = e->AddComponent<TransformComponent>(); t->position = r.pos; t->rotationEuler = r.rot; t->scale = r.scale; }
            if (r.hasMesh) { auto* m = e->AddComponent<MeshRendererComponent>(); m->assetId = r.meshAssetId; m->color = r.meshColor; m->visible = r.meshVisible; }
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
        EntityRecord r = Capture(s);

        EntityId id = world.CreateEntity(r.name + " Copy");
        Entity* e = world.Get(id);
        if (r.hasTransform) { auto* t = e->AddComponent<TransformComponent>(); t->position = r.pos; t->rotationEuler = r.rot; t->scale = r.scale; }
        if (r.hasMesh) { auto* m = e->AddComponent<MeshRendererComponent>(); m->assetId = r.meshAssetId; m->color = r.meshColor; m->visible = r.meshVisible; }
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
    namespace {
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
    }

    void EditorRenameEntity(World& world, CommandHistory& history, EntityId id,
        const std::string& oldName, const std::string& newName)
    {
        Entity* e = world.Get(id);
        if (!e || newName.empty() || oldName == newName) return;
        e->name = newName;
        history.Push(std::make_unique<RenameCommand>(world, id, oldName, newName));
    }
} // namespace uel