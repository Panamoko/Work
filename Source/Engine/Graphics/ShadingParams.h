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

        // 材質。
        float metallic = 0.0f;              // 金属らしさ。上げると下地の色が消え、ハイライトと空の映り込みに下地の色が付く
        float roughness = 0.5f;             // 表面の粗さ。小さいほどハイライトが小さく鋭く、映り込みがはっきりする

        // スペキュラ（ハイライト）の強さ。0 なら出ない。大きさは roughness で決まる。
        float specularStrength = 0.0f;

        // リムライト（輪郭が光る）。strength が 0 なら出ない。
        float             rimStrength = 0.0f;
        float             rimPower = 3.0f;  // 大きいほど縁だけに細く出る
        DirectX::XMFLOAT3 rimColor{ 1.0f, 1.0f, 1.0f };

        // Toon 専用。
        float             toonThreshold = 0.5f;   // 明暗の境目の位置。0.5 で光と直角の面が境目。上げると陰が広がる
        float             toonSoftness = 0.02f;   // 境目のぼかし幅。小さいほどくっきり
        DirectX::XMFLOAT3 toonShadeColor{ 0.6f, 0.6f, 0.7f };   // 陰の面に掛ける色（明るい面に対する倍率）

        // Toon 専用：テクスチャの単純化（リアル寄りのテクスチャを塗りっぽくする）。どちらも 0 で無効。
        float toonTexSteps = 0.0f;    // テクスチャの明るさを何段階に丸めるか（2 以上で有効。小さいほど大ざっぱ）
        float toonTexSmooth = 0.0f;   // 丸める前に周りの色とならす半径（UV の長さ。0.005 くらいから）

        // アウトライン（輪郭線）。どの mode でも使える。width が 0 なら描かない。
        float             outlineWidth = 0.0f;    // 太さ（ワールドの長さ。0.02 で 2cm）
        DirectX::XMFLOAT3 outlineColor{ 0.05f, 0.05f, 0.05f };
    };

} // namespace uel