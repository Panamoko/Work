#ifndef COMMON_HLSLI
#define COMMON_HLSLI

// メッシュの描画（静的・スキンド）で共通に使う宣言。
//   ・b0 の定数バッファ ObjectCB
//   ・テクスチャとサンプラ
//   ・頂点シェーダの出力（PSIn）と、それを作る関数（通常用 / アウトライン用）
//   ・ピクセルシェーダの出力（PSOut。色と法線の2枚）

// b0：オブジェクト＋ライト＋描き方。
// C++ 側（ShaderCommon.h の ObjectCB）と、並び・型を必ず一致させること。
cbuffer ObjectCB : register(b0)
{
    float4x4 world;
    float4x4 viewProj;
    float4x4 lightViewProj;
    float4 color;
    float4 lightDir;
    float4 lightColor;
    float4 params; // x = 環境光、y = 光の強さ、z = 影あり
    float4 cameraPos; // xyz = カメラのワールド位置
    float4 shading; // x = 描き方（0 = Lit, 1 = Unlit, 2 = Toon）、y = 影を受ける、z = metallic
    float4 specRim; // x = スペキュラの強さ、y = roughness、z = リムの強さ、w = リムの鋭さ
    float4 rimColor; // rgb = リムライトの色
    float4 toon; // x = 明暗の境目、y = ぼかし幅、z = テクスチャの段数、w = テクスチャをならす半径（UV）
    float4 toonShade; // rgb = 陰の面に掛ける色
    float4 outline; // rgb = アウトラインの色、a = 太さ（ワールドの長さ）
};

Texture2D tex : register(t0); // メッシュのテクスチャ（無ければ 1x1 の白）
Texture2D shadowMap : register(t1); // シャドウマップ（ライトから見た深度）
SamplerState samp : register(s0); // 通常（線形・繰り返し）
SamplerComparisonState shadowSamp : register(s1); // 影用（深度を比べた結果を返す）

// 頂点シェーダ → ピクセルシェーダ
struct PSIn
{
    float4 pos : SV_POSITION;
    float3 nrm : NORMAL; // ワールド法線（長さは 1 とは限らない）
    float4 col : COLOR; // 頂点カラー
    float2 uv : TEXCOORD;
    float3 wpos : WPOS; // ワールド位置
};

// ---- 頂点シェーダ用 ----
// ワールド位置とワールド法線から、ピクセルシェーダへ渡す値を作る。
PSIn MakePSIn(float3 wpos, float3 wnrm, float4 col, float2 uv)
{
    PSIn o;
    o.wpos = wpos;
    o.pos = mul(float4(wpos, 1.0), viewProj);
    o.nrm = wnrm;
    o.col = col;
    o.uv = uv;
    return o;
}

// アウトライン（背面法）：頂点を法線の向きに押し出す。
// このパスは裏面だけを描くので、ふくらんだ形が元の形の外にはみ出た部分だけが見え、輪郭線になる。
PSIn MakeOutlinePSIn(float3 wpos, float3 wnrm, float4 col, float2 uv)
{
    float len = length(wnrm);
    float3 n = (len > 1e-5) ? (wnrm / len) : float3(0.0, 0.0, 0.0);
    return MakePSIn(wpos + n * outline.a, n, col, uv);
}

// ---- ピクセルシェーダの出力 ----
// シーンは2枚の描画先に同時に書く：
//   0 枚目 = 色、1 枚目 = ワールド法線（あとでポストプロセスが輪郭線を探すのに読む）
// 色の1枚しかつないでいないとき（空を描く間など）は、2枚目の出力は捨てられるだけ。
struct PSOut
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

PSOut MakePSOut(float3 c, float3 worldNormal)
{
    PSOut o;
    o.color = float4(c, 1.0);
    o.normal = float4(normalize(worldNormal) * 0.5 + 0.5, 1.0); // a = 1 は「ここに物がある」の印
    return o;
}

#endif // COMMON_HLSLI
