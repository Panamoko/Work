#include "Engine/Serialization/StageSerializer.h"
#include "Engine/Scene/World.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Scene/HPComponent.h"
#include "Engine/Game/OpponentComponent.h"
#include "Engine/Game/PlayerComponent.h"
#include "Engine/Scene/SkinnedMeshComponent.h"
#include "Engine/AI/NavObstacleComponent.h"
#include "Engine/Game/ScreenUIComponent.h"
#include "Engine/Game/GameRuleComponent.h"
#include "Engine/Scene/PrefabInstanceComponent.h"
#include "Engine/Game/HitboxComponent.h"
#include "Engine/Game/SpawnerComponent.h"

#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
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

        // 読み込み中ファイルの schemaVersion（保存時は最新）。古い版に無い項目は読まない
        constexpr int kSchemaLatest = 10;
        int g_loadSchema = kSchemaLatest;

        struct EntityDTO {
            std::string name; TransformDTO transform;
            bool hasMesh = false; MeshDTO mesh;
            bool  hasHP = false; float hp = 100.0f; float maxHp = 100.0f;
            bool  hasOpponent = false; float oppMoveSpeed = 2.0f; float oppStopRange = 1.2f;
            bool  hasPlayer = false;
            bool  hasSkinned = false; std::string skAsset; int skClip = 0; float skSpeed = 1.0f;
            bool  skLoop = true; bool skVisible = true; Vec4DTO skColor;
            std::string skAnim;
            bool  hasNavObs = false; float navHalfX = 1.0f; float navHalfZ = 1.0f;
            bool  hasUI = false; std::string uiTitle, uiSub, uiButton, uiNext; bool uiShowResult = false;
            bool  hasRule = false; std::string ruleWin, ruleLose; float ruleDelay = 2.0f;
            std::string prefabPath;   // 空でなければPrefabのインスタンス
            bool  hasHitbox = false;
            float hbBodyBottom = 0.3f, hbBodyTop = 1.3f, hbBodyRadius = 0.35f;
            float hbHeadHeight = 1.6f, hbHeadRadius = 0.22f, hbHeadMul = 2.0f;
            bool  hasSpawner = false; std::string spPrefab;
            int   spWaves = 3, spBase = 3, spInc = 2;
            float spInterval = 0.8f, spFirstDelay = 1.0f, spWaveDelay = 3.0f, spRadius = 2.0f;
            template<class A> void serialize(A& ar) {
                ar(CEREAL_NVP(name), CEREAL_NVP(transform), CEREAL_NVP(hasMesh), CEREAL_NVP(mesh),
                    CEREAL_NVP(hasHP), CEREAL_NVP(hp), CEREAL_NVP(maxHp),
                    CEREAL_NVP(hasOpponent), CEREAL_NVP(oppMoveSpeed), CEREAL_NVP(oppStopRange),
                    CEREAL_NVP(hasPlayer));                                                          // v3
                if (g_loadSchema >= 4)
                    ar(CEREAL_NVP(hasSkinned), CEREAL_NVP(skAsset), CEREAL_NVP(skClip), CEREAL_NVP(skSpeed),
                        CEREAL_NVP(skLoop), CEREAL_NVP(skVisible), CEREAL_NVP(skColor));            // v4
                if (g_loadSchema >= 5) ar(CEREAL_NVP(skAnim));                                       // v5
                if (g_loadSchema >= 6) ar(CEREAL_NVP(hasNavObs), CEREAL_NVP(navHalfX), CEREAL_NVP(navHalfZ)); // v6
                if (g_loadSchema >= 7)
                    ar(CEREAL_NVP(hasUI), CEREAL_NVP(uiTitle), CEREAL_NVP(uiSub), CEREAL_NVP(uiButton),
                        CEREAL_NVP(uiNext), CEREAL_NVP(uiShowResult),
                        CEREAL_NVP(hasRule), CEREAL_NVP(ruleWin), CEREAL_NVP(ruleLose), CEREAL_NVP(ruleDelay)); // v7
                if (g_loadSchema >= 8) ar(CEREAL_NVP(prefabPath));                                   // v8
                if (g_loadSchema >= 9)
                    ar(CEREAL_NVP(hasHitbox), CEREAL_NVP(hbBodyBottom), CEREAL_NVP(hbBodyTop), CEREAL_NVP(hbBodyRadius),
                        CEREAL_NVP(hbHeadHeight), CEREAL_NVP(hbHeadRadius), CEREAL_NVP(hbHeadMul));   // v9
                if (g_loadSchema >= 10)
                    ar(CEREAL_NVP(hasSpawner), CEREAL_NVP(spPrefab), CEREAL_NVP(spWaves), CEREAL_NVP(spBase), CEREAL_NVP(spInc),
                        CEREAL_NVP(spInterval), CEREAL_NVP(spFirstDelay), CEREAL_NVP(spWaveDelay), CEREAL_NVP(spRadius));   // v10
            }
        };

        // Entity → 保存データ（Scene保存とPrefab保存で共用）
        EntityDTO ToDTO(const Entity& e, const TransformComponent& t)
        {
            EntityDTO ed;
            ed.name = e.name;
            ed.transform.position = { t.position.x, t.position.y, t.position.z };
            ed.transform.rotationEuler = { t.rotationEuler.x, t.rotationEuler.y, t.rotationEuler.z };
            ed.transform.scale = { t.scale.x, t.scale.y, t.scale.z };
            if (auto* m = e.GetComponent<MeshRendererComponent>()) {
                ed.hasMesh = true;
                ed.mesh.assetId = m->assetId;
                ed.mesh.color = { m->color.x, m->color.y, m->color.z, m->color.w };
                ed.mesh.visible = m->visible;
                ed.mesh.firstPerson = m->firstPerson;
                ed.mesh.hiddenSubmesh.assign(m->hiddenSubmesh.begin(), m->hiddenSubmesh.end());
                ed.mesh.submeshRole.assign(m->submeshRole.begin(), m->submeshRole.end());
            }
            if (auto* hpc = e.GetComponent<HPComponent>()) { ed.hasHP = true; ed.hp = hpc->hp; ed.maxHp = hpc->maxHp; }
            if (auto* op = e.GetComponent<OpponentComponent>()) { ed.hasOpponent = true; ed.oppMoveSpeed = op->moveSpeed; ed.oppStopRange = op->stopRange; }
            if (e.GetComponent<PlayerComponent>()) ed.hasPlayer = true;
            if (auto* sk = e.GetComponent<SkinnedMeshComponent>()) {
                ed.hasSkinned = true;
                ed.skAsset = sk->assetPath; ed.skClip = sk->clipIndex; ed.skSpeed = sk->speed;
                ed.skAnim = sk->animPath;
                ed.skLoop = sk->loop; ed.skVisible = sk->visible;
                ed.skColor = { sk->color.x, sk->color.y, sk->color.z, sk->color.w };
            }
            if (auto* ob = e.GetComponent<NavObstacleComponent>()) { ed.hasNavObs = true; ed.navHalfX = ob->halfX; ed.navHalfZ = ob->halfZ; }
            if (auto* ui = e.GetComponent<ScreenUIComponent>()) {
                ed.hasUI = true; ed.uiTitle = ui->title; ed.uiSub = ui->subtitle;
                ed.uiButton = ui->buttonText; ed.uiNext = ui->nextScene; ed.uiShowResult = ui->showResult;
            }
            if (auto* gr = e.GetComponent<GameRuleComponent>()) {
                ed.hasRule = true; ed.ruleWin = gr->winScene; ed.ruleLose = gr->loseScene; ed.ruleDelay = gr->endDelay;
            }
            if (auto* hb = e.GetComponent<HitboxComponent>()) {
                ed.hasHitbox = true;
                ed.hbBodyBottom = hb->bodyBottom; ed.hbBodyTop = hb->bodyTop; ed.hbBodyRadius = hb->bodyRadius;
                ed.hbHeadHeight = hb->headHeight; ed.hbHeadRadius = hb->headRadius; ed.hbHeadMul = hb->headMultiplier;
            }
            if (auto* sp = e.GetComponent<SpawnerComponent>()) {
                ed.hasSpawner = true; ed.spPrefab = sp->prefabPath;
                ed.spWaves = sp->waveCount; ed.spBase = sp->baseCount; ed.spInc = sp->countIncrease;
                ed.spInterval = sp->spawnInterval; ed.spFirstDelay = sp->firstDelay;
                ed.spWaveDelay = sp->waveDelay; ed.spRadius = sp->spawnRadius;
            }
            if (auto* pi = e.GetComponent<PrefabInstanceComponent>()) ed.prefabPath = pi->path;

            return ed;
        }

        // 保存データ → Entity にコンポーネントを付ける（名前は呼び出し側で設定済み）
        void FromDTO(const EntityDTO& ed, Entity& e)
        {
            auto* t = e.AddComponent<TransformComponent>();
            t->position = { ed.transform.position.x, ed.transform.position.y, ed.transform.position.z };
            t->rotationEuler = { ed.transform.rotationEuler.x, ed.transform.rotationEuler.y, ed.transform.rotationEuler.z };
            t->scale = { ed.transform.scale.x, ed.transform.scale.y, ed.transform.scale.z };
            if (ed.hasMesh) {
                auto* m = e.AddComponent<MeshRendererComponent>();
                m->assetId = ed.mesh.assetId;
                m->color = { ed.mesh.color.x, ed.mesh.color.y, ed.mesh.color.z, ed.mesh.color.w };
                m->visible = ed.mesh.visible;
                m->firstPerson = ed.mesh.firstPerson;
                m->hiddenSubmesh.assign(ed.mesh.hiddenSubmesh.begin(), ed.mesh.hiddenSubmesh.end());
                m->submeshRole.assign(ed.mesh.submeshRole.begin(), ed.mesh.submeshRole.end());
            }
            if (ed.hasHP) { auto* hpc = e.AddComponent<HPComponent>(); hpc->hp = ed.hp; hpc->maxHp = ed.maxHp; }
            if (ed.hasOpponent) { auto* op = e.AddComponent<OpponentComponent>(); op->moveSpeed = ed.oppMoveSpeed; op->stopRange = ed.oppStopRange; }
            if (ed.hasPlayer)   e.AddComponent<PlayerComponent>();
            if (ed.hasSkinned) {
                auto* sk = e.AddComponent<SkinnedMeshComponent>();
                sk->assetPath = ed.skAsset; sk->clipIndex = ed.skClip; sk->speed = ed.skSpeed;
                sk->animPath = ed.skAnim;
                sk->loop = ed.skLoop; sk->visible = ed.skVisible;
                sk->color = { ed.skColor.x, ed.skColor.y, ed.skColor.z, ed.skColor.w };
            }
            if (ed.hasNavObs) { auto* ob = e.AddComponent<NavObstacleComponent>(); ob->halfX = ed.navHalfX; ob->halfZ = ed.navHalfZ; }
            if (ed.hasUI) {
                auto* ui = e.AddComponent<ScreenUIComponent>();
                ui->title = ed.uiTitle; ui->subtitle = ed.uiSub; ui->buttonText = ed.uiButton;
                ui->nextScene = ed.uiNext; ui->showResult = ed.uiShowResult;
            }
            if (ed.hasRule) {
                auto* gr = e.AddComponent<GameRuleComponent>();
                gr->winScene = ed.ruleWin; gr->loseScene = ed.ruleLose; gr->endDelay = ed.ruleDelay;
            }
            if (ed.hasHitbox) {
                auto* hb = e.AddComponent<HitboxComponent>();
                hb->bodyBottom = ed.hbBodyBottom; hb->bodyTop = ed.hbBodyTop; hb->bodyRadius = ed.hbBodyRadius;
                hb->headHeight = ed.hbHeadHeight; hb->headRadius = ed.hbHeadRadius; hb->headMultiplier = ed.hbHeadMul;
            }
            if (ed.hasSpawner) {
                auto* sp = e.AddComponent<SpawnerComponent>();
                sp->prefabPath = ed.spPrefab;
                sp->waveCount = ed.spWaves; sp->baseCount = ed.spBase; sp->countIncrease = ed.spInc;
                sp->spawnInterval = ed.spInterval; sp->firstDelay = ed.spFirstDelay;
                sp->waveDelay = ed.spWaveDelay; sp->spawnRadius = ed.spRadius;
            }
            if (!ed.prefabPath.empty()) e.AddComponent<PrefabInstanceComponent>()->path = ed.prefabPath;
        }

        // Entityの今のTransform（インスタンスごとに残す値）
        TransformDTO CurrentTransform(const Entity& e)
        {
            TransformDTO td;
            td.scale = { 1.0f, 1.0f, 1.0f };
            if (auto* t = e.GetComponent<TransformComponent>()) {
                td.position = { t->position.x, t->position.y, t->position.z };
                td.rotationEuler = { t->rotationEuler.x, t->rotationEuler.y, t->rotationEuler.z };
                td.scale = { t->scale.x, t->scale.y, t->scale.z };
            }
            return td;
        }

        bool LoadPrefabDTO(const std::string& path, EntityDTO& out)
        {
            std::ifstream ifs(path);
            if (!ifs) return false;
            int schemaVersion = 0;
            try {
                cereal::JSONInputArchive ar(ifs);
                ar(cereal::make_nvp("schemaVersion", schemaVersion));
                g_loadSchema = schemaVersion;
                ar(cereal::make_nvp("prefab", out));
            }
            catch (...) {
                g_loadSchema = kSchemaLatest;
                return false;
            }
            g_loadSchema = kSchemaLatest;
            return true;
        }

        // Prefabの中身でEntityを作り直す（Transformはインスタンス側の値を残す）
        void ApplyPrefab(const EntityDTO& prefab, const std::string& path, const TransformDTO& keep, Entity& e)
        {
            EntityDTO d = prefab;
            d.transform = keep;
            d.prefabPath = path;
            e.components.clear();
            FromDTO(d, e);
        }
    }

    bool StageSerializer::Save(World& world, const char* path)
    {
        std::vector<EntityDTO> entities;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            const Entity& e = *slot;
            auto* t = e.GetComponent<TransformComponent>();
            if (!t) continue; // 最低限 Transform があるものだけ保存
            entities.push_back(ToDTO(e, *t));   // インスタンスも全内容を保存（Prefabが無くなった時の保険）
        }

        std::ofstream ofs(path);
        if (!ofs) return false;
        int schemaVersion = kSchemaLatest;
        g_loadSchema = kSchemaLatest;
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
            ar(cereal::make_nvp("schemaVersion", schemaVersion));
            g_loadSchema = schemaVersion;                 // このファイルの版に存在する項目だけ読む
            ar(cereal::make_nvp("entities", entities));
        }
        catch (...) {
            g_loadSchema = kSchemaLatest;
            return false; // 壊れたJSON等は失敗扱い
        }
        g_loadSchema = kSchemaLatest;

        world.Clear();
        for (const auto& ed : entities) {
            EntityId id = world.CreateEntity(ed.name);
            Entity& e = *world.Get(id);
            EntityDTO pd;
            if (!ed.prefabPath.empty() && LoadPrefabDTO(ed.prefabPath, pd))
                ApplyPrefab(pd, ed.prefabPath, ed.transform, e);   // Prefabの最新内容＋インスタンスのTransform
            else
                FromDTO(ed, e);                                       // 通常Entity／Prefabが見つからない時は保存内容で復元
        }
        return true;
    }

    bool StageSerializer::SaveSceneList(const std::vector<std::string>& names, const char* path)
    {
        std::ofstream ofs(path);
        if (!ofs) return false;
        std::vector<std::string> list = names;
        cereal::JSONOutputArchive ar(ofs);
        ar(cereal::make_nvp("scenes", list));
        return true; // ar 破棄時に flush
    }

    bool StageSerializer::LoadSceneList(std::vector<std::string>& names, const char* path)
    {
        std::ifstream ifs(path);
        if (!ifs) return false;
        try {
            cereal::JSONInputArchive ar(ifs);
            ar(cereal::make_nvp("scenes", names));
        }
        catch (...) { return false; }
        return true;
    }

    bool StageSerializer::SavePrefab(const Entity& e, const char* path)
    {
        TransformComponent identity;                                   // Transform が無いEntity用
        const TransformComponent* t = e.GetComponent<TransformComponent>();
        EntityDTO ed = ToDTO(e, t ? *t : identity);
        ed.transform.position = { 0.0f, 0.0f, 0.0f };                  // 位置は配置時に決める
        ed.prefabPath.clear();                                         // Prefab自身はリンクを持たない

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);  // Data/Prefabs を自動作成

        std::ofstream ofs(path);
        if (!ofs) return false;
        int schemaVersion = kSchemaLatest;
        g_loadSchema = kSchemaLatest;
        cereal::JSONOutputArchive ar(ofs);
        ar(cereal::make_nvp("schemaVersion", schemaVersion),
            cereal::make_nvp("prefab", ed));
        return true;
    }

    bool StageSerializer::InstantiatePrefab(World& world, const char* path, EntityId& outId)
    {
        EntityDTO pd;
        if (!LoadPrefabDTO(path, pd)) return false;
        outId = world.CreateEntity(pd.name);
        ApplyPrefab(pd, path, pd.transform, *world.Get(outId));   // リンク付きで生成（位置は0）
        return true;
    }

    int StageSerializer::RefreshPrefabInstances(World& world, const char* path)
    {
        EntityDTO pd;
        if (!LoadPrefabDTO(path, pd)) return 0;
        int n = 0;
        for (const auto& slot : world.Slots()) {
            if (!slot) continue;
            Entity& e = *slot;
            auto* pi = e.GetComponent<PrefabInstanceComponent>();
            if (!pi || pi->path != path) continue;
            ApplyPrefab(pd, path, CurrentTransform(e), e);
            ++n;
        }
        return n;
    }

    bool StageSerializer::RevertPrefabInstance(Entity& e)
    {
        auto* pi = e.GetComponent<PrefabInstanceComponent>();
        if (!pi) return false;
        std::string path = pi->path;           // 部品を作り直すので先にコピー
        EntityDTO pd;
        if (!LoadPrefabDTO(path, pd)) return false;
        ApplyPrefab(pd, path, CurrentTransform(e), e);
        return true;
    }

} // namespace uel