#pragma once
#include "Engine/Animation/SkinnedModel.h"
#include <string>

namespace uel {

    // リグ＋アニメ入り .glb を読み込む（skin+meshを持つ最初のノードを対象）。
    bool LoadSkinnedModel(const std::string& path, SkinnedModel& out);

} // namespace uel