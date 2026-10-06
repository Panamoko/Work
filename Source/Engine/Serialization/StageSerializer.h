#pragma once
#include <vector>
#include <string>
#include "Engine/Scene/EntityId.h"

namespace uel {
    class World;
    class Entity;
    struct SceneLighting;
    struct PostProcessSettings;

    // Stage を JSON で保存/読込（仕様書12章）。
    class StageSerializer {
    public:
        static bool Save(World& world, const char* path);
        static bool Load(World& world, const char* path); // 読込前に world をクリアして再構築

        // Scene一覧（マニフェスト）
        static bool SaveSceneList(const std::vector<std::string>& names, const char* path);
        static bool LoadSceneList(std::vector<std::string>& names, const char* path);

        // Prefab（Entity 1つぶんのテンプレート）
        static bool SavePrefab(const Entity& e, const char* path);
        static bool InstantiatePrefab(World& world, const char* path, EntityId& outId);
        static int  RefreshPrefabInstances(World& world, const char* path);   // 同じPrefabのインスタンスを作り直す（戻り値=件数）
        static bool RevertPrefabInstance(Entity& e);                           // 1体をPrefabの内容に戻す

        // Environment（ライトとポストプロセスの設定）。Scene ごとではなく、プロジェクトで1つ。
        // 読み込みに失敗したとき（ファイルが無い・壊れている）は false を返し、light / post は書き換えない。
        static bool SaveEnvironment(const SceneLighting& light, const PostProcessSettings& post, const char* path);
        static bool LoadEnvironment(SceneLighting& light, PostProcessSettings& post, const char* path);
    };

} // namespace uel