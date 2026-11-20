#pragma once
#include "Engine/Components/Component.h"

namespace uel {

    // 経路探索のグリッド（AI/EngineNavGrid）を作る範囲（5-4c）。UE の NavMeshBoundsVolume に当たる。
    // Entity の位置を真ん中にした箱の中に、cellSize の大きさのマスを並べる。Play を始めたとき（と Scene が変わったとき）に1回だけ作る。
    // マスごとに地面の高さを調べ（当たり判定なしのサブメッシュは飛ばす）、次のマスは「歩けない」にする：
    //   ・地面が無い、または地面が箱の高さの範囲の外（海の底の床など）
    //   ・隣のマスとの段差が maxStep より大きい（崖）
    // この部品が無い Scene は、今までどおり原点を真ん中にした 60m 四方（地面は見ない）。
    // 動きは無いので Update は持たない（ViewportPanel がグリッドを作るときに読む）。
    class NavBoundsComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<NavBoundsComponent>(*this); }

        // ---- 設定（保存する。版 28）----
        float halfX = 50.0f;        // 箱の横の半分（m）
        float halfY = 10.0f;        // 箱の高さの半分（m）。地面がこの範囲の外のマスは歩けない
        float halfZ = 50.0f;        // 箱の奥の半分（m）
        float cellSize = 1.0f;      // マスの大きさ（m）
        float maxStep = 1.0f;       // 隣のマスへ登り降りできる段差（m）。キャラは足元より 1.5m 上の面まで乗れるので、それより小さく
    };

} // namespace uel
