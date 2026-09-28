#pragma once
#include <vector>
#include <string>

namespace uel {
    class World;

    // Stage を JSON で保存/読込（仕様書12章）。
    class StageSerializer {
    public:
        static bool Save(World& world, const char* path);
        static bool Load(World& world, const char* path); // 読込前に world をクリアして再構築

        // Scene一覧（マニフェスト）
        static bool SaveSceneList(const std::vector<std::string>& names, const char* path);
        static bool LoadSceneList(std::vector<std::string>& names, const char* path);
    };

} // namespace uel