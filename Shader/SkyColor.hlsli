#ifndef SKY_COLOR_HLSLI
#define SKY_COLOR_HLSLI

// 空の色。
//   SkyColor(dir)    : 上下のグラデーション（空の画像が読めなかったときの代わり。今は金属の映り込みにも使う）
//   HdrToScreen(hdr) : 空の画像（HDR）の色を、画面に出せる 0〜1 の色に直す

static const float3 kSkyHorizon = float3(0.55, 0.65, 0.78); // 地平線
static const float3 kSkyZenith = float3(0.13, 0.28, 0.55); // 真上
static const float3 kSkyGround = float3(0.18, 0.20, 0.23); // 真下（地面側）

// 見ている向き dir（長さ 1）の空の色。上を向くほど真上の色、下を向くほど地面側の色に近づく。
float3 SkyColor(float3 dir)
{
    return (dir.y >= 0.0) ? lerp(kSkyHorizon, kSkyZenith, saturate(dir.y))
                          : lerp(kSkyHorizon, kSkyGround, saturate(-dir.y));
}

// 空の画像の明るさの倍率（露出）。上げると明るく、下げると暗くなる
static const float kSkyExposure = 1.0;

// HDR の色 → 画面の色。
//   HDR の画像は「光の量」をそのまま持っていて、上限が無い（雲は 1 前後、太陽は数万）。
//   1. 露出を掛ける
//   2. トーンマップ（ACES の近似式）：暗いところはほぼそのまま、明るいところはなめらかに 1 へ寄せる
//   3. ガンマ補正：光の量 → 画面の明るさ（約 1/2.2 乗）。このエンジンの他の色（テクスチャなど）は
//      画面の明るさのまま計算しているので、それに合わせる
float3 HdrToScreen(float3 hdr)
{
    float3 x = max(hdr, 0.0) * kSkyExposure;
    float3 t = saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
    return pow(t, 1.0 / 2.2);
}

#endif // SKY_COLOR_HLSLI