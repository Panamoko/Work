#pragma once
#include "Engine/Animation/AnimNode.h"
#include "Engine/Animation/AnimParams.h"
#include "Engine/Animation/AnimMontage.h"
#include <string>
#include <vector>

namespace uel {

    // アニメーショングラフ（パラメータの一覧＋ノードの木＋モンタージュの定義）を JSON で保存・読み込みする。
    // 置き場所は Data/Anim/<名前>.animgraph.json。
    // グラフをファイルにしておくと、同じグラフを複数のキャラ（Prefab）で使い回せる。

    // 保存する。フォルダが無ければ作る。params の今の値が、読み込んだときの初期値になる。
    bool SaveAnimGraph(const std::string& path, const AnimParams& params, const AnimNode* root, const std::vector<AnimMontage>& montages);

    // 読み込む。成功したときだけ params と root と montages を置き換える（失敗したら元のまま）。
    bool LoadAnimGraph(const std::string& path, AnimParams& params, AnimNodePtr& root, std::vector<AnimMontage>& montages);

    // 名前だけ（"Enemy"）を渡すと、決まった置き場所のパス（"Data/Anim/Enemy.animgraph.json"）にして返す。
    // すでにパスの形（'/' を含む、または拡張子付き）ならそのまま返す。
    std::string AnimGraphPathFromName(const std::string& nameOrPath);

} // namespace uel
