#pragma once

namespace uel {
    class World;

    // Stage を JSON で保存/読込（仕様書12章）。
    class StageSerializer {
    public:
        static bool Save(World& world, const char* path);
        static bool Load(World& world, const char* path); // 読込前に world をクリアして再構築
    };

} // namespace uel