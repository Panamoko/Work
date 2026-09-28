#pragma once
#include "Engine/Animation/SkinnedModel.h"
#include <string>

namespace uel {

    // リグ＋アニメ入り .glb を読み込む（skin+meshを持つ最初のノードを対象）。
    bool LoadSkinnedModel(const std::string& path, SkinnedModel& out);
    // 別ファイル(rig glb)のアニメだけ読み込み、ボーン名でリターゲットしてtargetに追加
    bool LoadAnimationsFromFile(const std::string& path, SkinnedModel& target);
} // namespace uel