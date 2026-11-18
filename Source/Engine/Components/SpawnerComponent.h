#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Scene/EntityId.h"
#include <string>
#include <vector>
#include <cstdint>

namespace uel {
    // Play中に Prefab から敵をウェーブ単位で湧かせる
    class SpawnerComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<SpawnerComponent>(*this); }
        void Update(GameContext& ctx) override;

        std::string prefabPath;          // 湧かせるPrefab（Asset BrowserからInspectorへドロップ可）
        int   waveCount = 3;             // ウェーブ数
        int   baseCount = 3;             // 1ウェーブ目の数
        int   countIncrease = 2;         // ウェーブごとの増加数
        float spawnInterval = 0.8f;      // 1体ずつ湧く間隔（秒）
        float firstDelay = 1.0f;         // 開始から1ウェーブ目まで
        float waveDelay = 3.0f;          // ウェーブ間の休憩
        float spawnRadius = 2.0f;        // この半径内のランダムな位置に出す
        // どの「止まる場所」で始まるか（5-2。保存する。版 25）。
        // 0 = Play の始めから動く（今までどおり）。1 以上 = 案内ロボット（GuideComponent）がその番号の止まる場所に着いたら始まる。
        // それまでは眠っている（終わった扱いにはしないので、GameRule は「まだウェーブが残っている」とみなす）
        int   activateAtStop = 0;

        int   CurrentWave() const { return wave; }
        int   AliveCount()  const { return (int)alive.size(); }
        bool  IsFinished()  const { return finished; }
        bool  IsDormant()   const { return !started; }    // まだ始まっていない（ロボットを待っている）
        bool  HasError()    const { return failed || errorText != nullptr; }
        // HUD に出すエラーの文（無ければ nullptr）。Prefab が読めない／ロボットがいない／その番号の止まる場所が道に無い
        const char* ErrorText() const { return failed ? "Spawner: prefab not found" : errorText; }
        float BannerTime()  const { return banner; }   // 「WAVE n」表示の残り時間

    private:
        bool  CheckActivation(GameContext& ctx);   // 始めてよいか（ロボットが止まる場所に着いたか）

        EntityId guideId;                // 案内ロボット（見つけたら覚えておく）
        float guideSearchTimer = 0.0f;   // ロボットが見つからないとき、次に探すまでの時間（毎フレームは探さない）
        const char* errorText = nullptr;
        enum class State { Waiting, Spawning, Fighting };
        State state = State::Waiting;
        bool  started = false;
        int   wave = 0;
        int   spawned = 0;
        float timer = 0.0f;
        float banner = 0.0f;
        bool  finished = false, failed = false;
        uint32_t rng = 2463534242u;
        std::vector<EntityId> alive;     // このSpawnerが出して、まだ生きている敵

        int   WaveSize() const { int n = baseCount + (wave - 1) * countIncrease; return n > 0 ? n : 1; }
        float Rand01();
    };
}