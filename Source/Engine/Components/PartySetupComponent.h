#pragma once
#include "Engine/Components/Component.h"
#include <string>

namespace uel {

    // Stage の開始時に、3人のチームをそろえる（UE の GameMode が Pawn を出して PlayerStart に置く処理に当たる）。
    // Play が始まった最初のフレームに1回だけ動く：
    //   1) プレイヤー（PlayerComponent を持つ Entity）に、選んだロール（GameFlow::playerRole）の Component を付ける
    //   2) 残りの2つのロールの AI 仲間を allyPrefab から作り、プレイヤーの左後ろ・右後ろに置く
    //   3) AI 仲間の名前を「Ally (Tank)」のようにする
    // Stage のどれか1つの Entity（GameRule の Entity など）に付けておく。
    class PartySetupComponent : public Component {
    public:
        std::unique_ptr<Component> Clone() const override { return std::make_unique<PartySetupComponent>(*this); }
        void Update(GameContext& ctx) override;

        // ---- 設定（保存する）----
        std::string allyPrefab = "Data/Prefabs/Ally.prefab.json";   // AI 仲間の Prefab
        float spawnBack = 2.0f;     // プレイヤーの何 m 後ろに出すか
        float spawnSide = 1.5f;     // プレイヤーの左右に何 m 離すか

        bool HasError() const { return failed; }   // Prefab が読めなかった（AI 仲間を出せなかった）

    private:
        bool done = false;      // もうそろえた（2回目からは何もしない）
        bool failed = false;
    };

} // namespace uel
