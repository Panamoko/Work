#pragma once
#include "Engine/Components/Component.h"
#include "Engine/Scene/EntityId.h"
#include <DirectXMath.h>
#include <vector>

namespace uel {
    class World;

    // 案内ロボット（5-1）。Scene に置いた経由点（GuidePointComponent）を order の順に、点から点へまっすぐ進む。
    // 「止まる場所」（stopHere）に着くと止まり、プレイヤーが近く（signalRange）で F を押すと次へ進む
    // （L4D のセーフルームの扉のように、次へ進むタイミングをプレイヤーが決める）。Stage の最初も、合図を待っている。
    // 5-2 から、止まる場所に着いたときにその場所のウェーブが始まり、倒すまでは合図を受け付けない。
    //
    // 現場での呼び方：経由点は UE の Target Point（またはスプライン）、合図はインタラクション（Interact）。
    // 道は Play の始めに1回だけ集めて、位置を覚えておく（経由点は動かない前提。毎フレーム Scene を探さない）。
    // 敵には狙われない（HP・陣営を持たない）。弾も止めない（MeshRenderer ではなく SkinnedMesh で見せる前提）。
    class GuideComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<GuideComponent>(*this); }
        void Update(GameContext& ctx) override;

        enum class State { Waiting, Moving, Finished };    // 合図を待っている／進んでいる／最後の点に着いた

        // ---- 設定（保存する。版 24）----
        float moveSpeed = 2.0f;     // 進む速さ（m/秒）。プレイヤーの歩き（4）より遅く、ついて行ける速さ
        float signalRange = 4.0f;   // プレイヤーが合図できる距離（m。水平）
        float markerHeight = 2.2f;  // HUD の印を出す高さ（m。Transform の Scale.y を掛ける。ロボットの頭の少し上）

        // ---- HUD・ほかの Component が読む値 ----
        State GetState() const { return state; }
        bool  CanSignal() const { return canSignal; }           // プレイヤーが近くにいて、今 F を押せば進む
        int   GetStopsReached() const { return stopsReached; }  // 着いた「止まる場所」の数（0 = まだ出発していない）
        int   GetStopCount() const { return stopCount; }        // 道にある「止まる場所」の数
        bool  HasError() const { return routeEmpty; }           // 経由点が1つも無い（進めない）

        // アニメーショングラフに渡すパラメータ（グラフにその名前があれば書く。無ければ何もしない）
        static constexpr const char* kAnimSpeed = "Speed";      // Float：進む速さ（m/秒。止まっていれば 0）
        static constexpr const char* kAnimWaiting = "Waiting";  // Bool ：合図を待っている

    private:
        void BuildRoute(World& world);

        struct RoutePoint { DirectX::XMFLOAT3 pos; bool stop; };

        // ---- 実行中の状態（保存しない）----
        bool  initialized = false;      // 道を集めた
        bool  routeEmpty = false;
        std::vector<RoutePoint> route;  // 通る順に並べた経由点（Play の始めに1回だけ作る）
        size_t next = 0;                // 次に向かう経由点の番号
        State state = State::Waiting;
        int   stopsReached = 0;
        int   stopCount = 0;
        bool  canSignal = false;
        EntityId playerId;              // プレイヤー（見つけたら覚えておく。毎フレーム探さない）
        float groundTimer = 0.0f;       // 次に地面の高さを調べるまでの残り時間
        float groundY = 0.0f;           // 地面の高さ（この高さへなめらかに寄せる）
        float speedAnim = -1.0f;        // アニメに渡した値（変わったときだけ書く）
        int   waitingAnim = -1;         // 同上（-1 = まだ書いていない）
    };

} // namespace uel
