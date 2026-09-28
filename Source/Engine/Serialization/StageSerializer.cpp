#include "Engine/Serialization/StageSerializer.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Scene/HPComponent.h"
#include "Engine/Game/OpponentComponent.h"
#include "Engine/Game/PlayerComponent.h"
#include "Engine/Scene/SkinnedMeshComponent.h"

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
            std::string      assetId;
            Vec4DTO          color;
            bool             visible = true;
            bool             firstPerson = false;
            std::vector<int> hiddenSubmesh;
            std::vector<int> submeshRole;
            template<class A> void serialize(A& ar) {
                ar(CEREAL_NVP(assetId), CEREAL_NVP(color), CEREAL_NVP(visible),
                    CEREAL_NVP(firstPerson), CEREAL_NVP(hiddenSubmesh), CEREAL_NVP(submeshRole));
            }
        };
        struct EntityDTO {
            std::string name; TransformDTO transform;
            std::string skAnim;
            bool hasMesh = false; MeshDTO mesh;
            bool  hasHP = false; float hp = 100.0f; float maxHp = 100.0f;
            bool  hasOpponent = false; float oppMoveSpeed = 2.0f; float oppStopRange = 1.2f;
            bool  hasPlayer = false;
            bool  hasSkinned = false; std::string skAsset; int skClip = 0; float skSpeed = 1.0f;
            bool  skLoop = true; bool skVisible = true; Vec4DTO skColor;
            template<class A> void serialize(A& ar) {
                ar(CEREAL_NVP(name), CEREAL_NVP(transform), CEREAL_NVP(hasMesh), CEREAL_NVP(mesh),
                    CEREAL_NVP(hasHP), CEREAL_NVP(hp), CEREAL_NVP(maxHp),
                    CEREAL_NVP(hasOpponent), CEREAL_NVP(oppMoveSpeed), CEREAL_NVP(oppStopRange),
                    CEREAL_NVP(hasPlayer),
                    CEREAL_NVP(hasSkinned), CEREAL_NVP(skAsset), CEREAL_NVP(skClip), CEREAL_NVP(skSpeed),
                    CEREAL_NVP(skLoop), CEREAL_NVP(skVisible), CEREAL_NVP(skColor), CEREAL_NVP(skAnim));
            }
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
                ed.mesh.firstPerson = m->firstPerson;
                ed.mesh.hiddenSubmesh.assign(m->hiddenSubmesh.begin(), m->hiddenSubmesh.end());
                ed.mesh.submeshRole.assign(m->submeshRole.begin(), m->submeshRole.end());
            }
            if (auto* hpc = e->GetComponent<HPComponent>()) { ed.hasHP = true; ed.hp = hpc->hp; ed.maxHp = hpc->maxHp; }
            if (auto* op = e->GetComponent<OpponentComponent>()) { ed.hasOpponent = true; ed.oppMoveSpeed = op->moveSpeed; ed.oppStopRange = op->stopRange; }
            if (e->GetComponent<PlayerComponent>()) ed.hasPlayer = true;
            if (auto* sk = e->GetComponent<SkinnedMeshComponent>()) {
                ed.hasSkinned = true;
                ed.skAsset = sk->assetPath; ed.skClip = sk->clipIndex; ed.skSpeed = sk->speed;
                ed.skAnim = sk->animPath;
                ed.skLoop = sk->loop; ed.skVisible = sk->visible;
                ed.skColor = { sk->color.x, sk->color.y, sk->color.z, sk->color.w };
            }

            entities.push_back(std::move(ed));
        }

        std::ofstream ofs(path);
        if (!ofs) return false;
        int schemaVersion = 5;
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
                m->firstPerson = ed.mesh.firstPerson;
                m->hiddenSubmesh.assign(ed.mesh.hiddenSubmesh.begin(), ed.mesh.hiddenSubmesh.end());
                m->submeshRole.assign(ed.mesh.submeshRole.begin(), ed.mesh.submeshRole.end());
            }
            if (ed.hasHP) { auto* hpc = e->AddComponent<HPComponent>(); hpc->hp = ed.hp; hpc->maxHp = ed.maxHp; }
            if (ed.hasOpponent) { auto* op = e->AddComponent<OpponentComponent>(); op->moveSpeed = ed.oppMoveSpeed; op->stopRange = ed.oppStopRange; }
            if (ed.hasPlayer)   e->AddComponent<PlayerComponent>();
            if (ed.hasSkinned) {
                auto* sk = e->AddComponent<SkinnedMeshComponent>();
                sk->assetPath = ed.skAsset; sk->clipIndex = ed.skClip; sk->speed = ed.skSpeed;
                sk->animPath = ed.skAnim;
                sk->loop = ed.skLoop; sk->visible = ed.skVisible;
                sk->color = { ed.skColor.x, ed.skColor.y, ed.skColor.z, ed.skColor.w };
            }
        }
        return true;
    }

} // namespace uel