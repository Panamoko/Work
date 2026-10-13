#pragma once
#include "Engine/Scene/EntityId.h"

struct ID3D11Device;

namespace uel {
    class World;

    // アニメーションのノードエディタ。
    // 選択中の Entity の AnimatorComponent が持つステートマシンを、状態（箱）と遷移（矢印）で表示・編集する。
    //   左：キャンバス（選ぶ、動かす、右クリックで足す・消す）
    //   右：選んでいる状態／遷移の編集欄
    // playMode は表示だけに使う（Play 中は、実行中の World の状態をそのまま見せる）。
    // device は、クリップの名前を選ぶためにモデルを取り出すのに使う。
    void DrawAnimGraphPanel(World& world, EntityId selected, bool playMode, ID3D11Device* device);

} // namespace uel