#pragma once

// 「どう描くか」を表すデータ。描画側（Graphics）が受け取る形。
// Entity にこの値を持たせる役は ShaderComponent（Scene 側）が担当する。
// Graphics は Component を知らず、RenderSystem が ShaderComponent から取り出して渡す。

namespace uel {

    // 描き方の種類。
    // ShaderCommon.h の HLSL（PSMain の mode 判定）と値を必ず一致させること。
    enum class ShadingMode : int {
        Lit = 0,    // アンビエント＋平行光＋影（今までと同じ）
        Unlit = 1,  // ライトの影響を受けない（テクスチャ × 色 そのまま）
    };

    struct ShadingParams {
        ShadingMode mode = ShadingMode::Lit;
        bool        receiveShadow = true;   // false にすると、他の物の影が落ちない
    };

} // namespace uel