#pragma once
#include <DirectXMath.h>

// 「どう描くか」を表すデータ。描画側（Graphics）が受け取る形。
// Entity にこの値を持たせる役は ShaderComponent（Scene 側）が担当する。
// Graphics は Component を知らず、RenderSystem が ShaderComponent から取り出して渡す。

namespace uel {

    // 描き方の種類。
    // ShaderCommon.h の HLSL（PSMain の mode 判定）と値を必ず一致させること。
    // 数値のまま保存しているので、並び順は変えない（増やすときは末尾に足す）。
    enum class ShadingMode : int {
        Lit = 0,    // アンビエント＋平行光＋影（今までと同じ）
        Unlit = 1,  // ライトの影響を受けない（テクスチャ × 色 そのまま）
        Toon = 2,   // 明るい面と陰の面の2色に分ける。スペキュラとリムもくっきりさせる
    };

    struct ShadingParams {
        ShadingMode mode = ShadingMode::Lit;
        bool        receiveShadow = true;   // false にすると、他の物の影が落ちない

        // スペキュラ（ハイライト）。strength が 0 なら出ない。
        float specularStrength = 0.0f;
        float specularPower = 32.0f;        // 大きいほど小さく鋭いハイライトになる

        // リムライト（輪郭が光る）。strength が 0 なら出ない。
        float             rimStrength = 0.0f;
        float             rimPower = 3.0f;  // 大きいほど縁だけに細く出る
        DirectX::XMFLOAT3 rimColor{ 1.0f, 1.0f, 1.0f };

        // Toon 専用。
        float             toonThreshold = 0.5f;   // 明暗の境目の位置。0.5 で光と直角の面が境目。上げると陰が広がる
        float             toonSoftness = 0.02f;   // 境目のぼかし幅。小さいほどくっきり
        DirectX::XMFLOAT3 toonShadeColor{ 0.6f, 0.6f, 0.7f };   // 陰の面に掛ける色（明るい面に対する倍率）

        // アウトライン（輪郭線）。どの mode でも使える。width が 0 なら描かない。
        float             outlineWidth = 0.0f;    // 太さ（ワールドの長さ。0.02 で 2cm）
        DirectX::XMFLOAT3 outlineColor{ 0.05f, 0.05f, 0.05f };
    };

} // namespace uel