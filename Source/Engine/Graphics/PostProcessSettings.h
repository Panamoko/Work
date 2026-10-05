#pragma once

namespace uel {

    // Viewport に何を表示するか（確認用）。PostProcess.cpp の HLSL と値を一致させること。
    enum class PostDebugView : int {
        Final = 0,    // 通常の表示
        Normal = 1,   // 法線（シーンの描画で書き出したもの）
        Depth = 2,    // 深度（カメラからの距離。近いほど暗い）
        Edge = 3,     // 検出した輪郭線だけ（白地に黒）。しきい値の調整用
    };

    // 画面全体に掛ける後処理（ポストプロセス）の設定。
    // Engine が1つ持ち、Environment パネルで編集して、ViewportPanel 経由で PostProcess に渡す。
    struct PostProcessSettings {
        float         saturation = 1.0f;   // 彩度。1 でそのまま、0 で白黒、1 より大きいと鮮やかになる
        PostDebugView debugView = PostDebugView::Final;

        // エッジ検出の輪郭線。深度と法線が急に変わる場所を線にする（画面全体・どのメッシュにも出る）。
        bool  edgeEnabled = false;
        float edgeColor[3] = { 0.05f, 0.05f, 0.05f };
        float edgeThickness = 1.0f;          // 線の太さの目安（ピクセル。比べる相手までの距離）
        float edgeDepthThreshold = 0.02f;    // 奥行きの段差がこれを超えたら線にする（距離に対する割合。0.02 = 2%）
        float edgeNormalThreshold = 0.3f;    // 面の向きの差がこれを超えたら線にする（0.3 でおよそ45度の折れ目）
        float edgeFadeDistance = 60.0f;      // これより遠くには線を出さない（手前7割の距離から薄くなる）
    };

} // namespace uel