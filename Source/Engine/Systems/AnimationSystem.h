#pragma once

struct ID3D11Device;
namespace uel {
    class World;
    class SkinnedModelCache;

    // スキン付きメッシュの「時間を進める → 姿勢を計算する」を、1フレームに1回だけ行う。
    // 結果は SkinnedMeshComponent の boneGlobal / palette に入り、影の描画と本描画の両方がそれを使う。
    // 呼ぶ場所は、ゲームの更新（Component の Update）と各パネルの後、描画の前。
    class AnimationSystem {
    public:
        static void Update(World& world, float dt, SkinnedModelCache& cache, ID3D11Device* device);
    };

} // namespace uel