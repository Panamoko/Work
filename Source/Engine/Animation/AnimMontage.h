#pragma once
#include "Engine/Animation/AnimNode.h"
#include <string>
#include <vector>

namespace uel {

    // モンタージュの定義（UE の Anim Montage アセットに当たる）。
    // グラフの上に1回だけ重ねて再生するクリップと、その入り方・抜け方・通知をまとめて名前を付けたもの。
    // グラフのアセット（Data/Anim/<名前>.animgraph.json）に一緒に保存する。
    //
    // ゲーム側は AnimatorComponent::PlayMontage("HitReact") のように「名前」で呼ぶ。
    // どのクリップを何秒で混ぜるかはグラフ側で決めるので、ゲーム側のコードはクリップを知らなくてよい。
    struct AnimMontage {
        std::string name;               // ゲーム側が呼ぶ名前（"HitReact" "Melee" など）
        std::string clipName;           // 再生するクリップ（名前で指定）
        float blendIn = 0.1f;           // 入るのにかける秒数
        float blendOut = 0.2f;          // クリップの終わりで抜けるのにかける秒数
        float rate = 1.0f;              // 再生速度
        std::vector<AnimClipNode::Notify> notifies;     // 通知（クリップの頭からの秒数）
    };

} // namespace uel