#ifndef POST_PROCESS_HLSLI
#define POST_PROCESS_HLSLI

// ポストプロセス（画面全体の後処理）の VS と PS で共通に使う宣言。

// b0：C++ 側（PostProcess.cpp の PostCB）と、並び・型を必ず一致させること。
cbuffer PostCB : register(b0)
{
    float4 grade; // x = 彩度、y = 確認用の表示（0 = 完成, 1 = 法線, 2 = 深度, 3 = エッジ）
    float4 depth; // x = proj._33、y = proj._43（深度を距離に戻す係数）、z = 1 / 幅、w = 1 / 高さ
    float4 edgeColor; // rgb = 輪郭線の色、a = 有効なら 1
    float4 edgeParams; // x = 太さ（ピクセル）、y = 深度のしきい値、z = 法線のしきい値、w = 線を出す最大距離
};

// シーンを描いた結果（RenderTexture のシーン用）
Texture2D sceneTex : register(t0); // 色
Texture2D normalTex : register(t1); // rgb = ワールド法線 × 0.5 + 0.5、a = 1 なら物がある（0 なら空）
Texture2D depthTex : register(t2); // 深度バッファの値（0 = 手前 〜 1 = 奥）
SamplerState samp : register(s0); // ポイント（補間なし）・端は引き伸ばし

struct PostVSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD; // 画面上の位置（左上 0,0 〜 右下 1,1）
};

#endif // POST_PROCESS_HLSLI
