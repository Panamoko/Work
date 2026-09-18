#include "Engine/Serialization/StageSerializer.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"

#include <fstream>
#include <vector>
#include <string>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

namespace uel {
    namespace {
        struct Vec3DTO {
            float x = 0, y = 0, z = 0;
            template<class A> void serialize(A& ar) { ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(z)); }
        };
        struct Vec4DTO {
            float x = 0, y = 0, z = 0, w = 1;
            template<class A> void serialize(A& ar) { ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(z), CEREAL_NVP(w)); }
        };
        struct TransformDTO {
            Vec3DTO position, rotationEuler, scale;
            template<class A> void serialize(A& ar) { ar(CEREAL_NVP(position), CEREAL_NVP(rotationEuler), CEREAL_NVP(scale)); }
        };
        struct MeshDTO {
            std::string assetId; Vec4DTO color; bool visible = true;
            template<class A> void serialize(A& ar) { ar(CEREAL_NVP(assetId), CEREAL_NVP(color), CEREAL_NVP(visible)); }
        };
        struct EntityDTO {
            std::string name; TransformDTO transform; bool hasMesh = false; MeshDTO mesh;
            template<class A> void serialize(A& ar) { ar(CEREAL_NVP(name), CEREAL_NVP(transform), CEREAL_NVP(hasMesh), CEREAL_NVP(mesh)); }
        };
    }

    bool StageSerializer::Save(World& world, const char* path)
    {
        std::vector<EntityDTO> entities;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            Entity* e = slot.get();
            auto* t = e->GetComponent<TransformComponent>();
            if (!t) continue; // 最低限 Transform を持つものだけ保存
            EntityDTO ed;
            ed.name = e->name;
            ed.transform.position = { t->position.x, t->position.y, t->position.z };
            ed.transform.rotationEuler = { t->rotationEuler.x, t->rotationEuler.y, t->rotationEuler.z };
            ed.transform.scale = { t->scale.x, t->scale.y, t->scale.z };
            if (auto* m = e->GetComponent<MeshRendererComponent>()) {
                ed.hasMesh = true;
                ed.mesh.assetId = m->assetId;
                ed.mesh.color = { m->color.x, m->color.y, m->color.z, m->color.w };
                ed.mesh.visible = m->visible;
            }
            entities.push_back(std::move(ed));
        }

        std::ofstream ofs(path);
        if (!ofs) return false;
        int schemaVersion = 1;
        cereal::JSONOutputArchive ar(ofs);
        ar(cereal::make_nvp("schemaVersion", schemaVersion),
            cereal::make_nvp("entities", entities));
        return true; // ar 破棄時に flush → ofs クローズ
    }

    bool StageSerializer::Load(World& world, const char* path)
    {
        std::ifstream ifs(path);
        if (!ifs) return false;

        int schemaVersion = 0;
        std::vector<EntityDTO> entities;
        try {
            cereal::JSONInputArchive ar(ifs);
            ar(cereal::make_nvp("schemaVersion", schemaVersion),
                cereal::make_nvp("entities", entities));
        }
        catch (...) {
            return false; // 壊れたJSON等は失敗扱い
        }

        // 将来：schemaVersion によるMigration（今は v1 のみ）

        world.Clear();
        for (const auto& ed : entities) {
            EntityId id = world.CreateEntity(ed.name);
            Entity* e = world.Get(id);
            auto* t = e->AddComponent<TransformComponent>();
            t->position = { ed.transform.position.x, ed.transform.position.y, ed.transform.position.z };
            t->rotationEuler = { ed.transform.rotationEuler.x, ed.transform.rotationEuler.y, ed.transform.rotationEuler.z };
            t->scale = { ed.transform.scale.x, ed.transform.scale.y, ed.transform.scale.z };
            if (ed.hasMesh) {
                auto* m = e->AddComponent<MeshRendererComponent>();
                m->assetId = ed.mesh.assetId;
                m->color = { ed.mesh.color.x, ed.mesh.color.y, ed.mesh.color.z, ed.mesh.color.w };
                m->visible = ed.mesh.visible;
            }
        }
        return true;
    }

} // namespace uel