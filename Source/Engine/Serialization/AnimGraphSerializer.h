#pragma once
#include "Engine/Animation/AnimNode.h"
#include "Engine/Animation/AnimParams.h"
#include <string>

namespace uel {

    // アニメーショングラフ（パラメータの一覧＋ノードの木）を JSON で保存・読み込みする。
    // 置き場所は Data/Anim/<名前>.animgraph.json。
    // グラフをファイルにしておくと、同じグラフを複数のキャラ（Prefab）で使い回せる。

    // 保存する。フォルダが無ければ作る。params の今の値が、読み込んだときの初期値になる。
    bool SaveAnimGraph(const std::string& path, const AnimParams& params, const AnimNode* root);

    // 読み込む。成功したときだけ params と root を置き換える（失敗したら元のまま）。
    bool LoadAnimGraph(const std::string& path, AnimParams& params, AnimNodePtr& root);

    // 名前だけ（"Enemy"）を渡すと、決まった置き場所のパス（"Data/Anim/Enemy.animgraph.json"）にして返す。
    // すでにパスの形（'/' を含む、または拡張子付き）ならそのまま返す。
    std::string AnimGraphPathFromName(const std::string& nameOrPath);

} // namespace uel