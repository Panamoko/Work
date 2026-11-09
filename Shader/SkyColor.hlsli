#ifndef SKY_COLOR_HLSLI
#define SKY_COLOR_HLSLI

// 空の色。空の描画（SkyboxPS.hlsl）と、メッシュの映り込み（Lighting.hlsli）の両方が使う。
//   SkyColor(dir)              : 上下のグラデーション（空の画像が読めなかったときの代わり）
//   RotateSkyDir(dir, cs)      : 空を回したときに、画像を読む向き
//   HdrToScreen(hdr, exposure) : 空の画像（HDR）の色を、画面に出せる 0〜1 の色に直す

static const float3 kSkyHorizon = float3(0.55, 0.65, 0.78); // 地平線
static const float3 kSkyZenith = float3(0.13, 0.28, 0.55); // 真上
static const float3 kSkyGround = float3(0.18, 0.20, 0.23); // 真下（地面側）

// 見ている向き dir（長さ 1）の空の色。上を向くほど真上の色、下を向くほど地面側の色に近づく。
float3 SkyColor(float3 dir)
{
    return (dir.y >= 0.0) ? lerp(kSkyHorizon, kSkyZenith, saturate(dir.y))
                          : lerp(kSkyHorizon, kSkyGround, saturate(-dir.y));
}

// 空を Y 軸まわりに回したときに、画像のどの向きを読めばよいか。cs = (cos, sin)（回転の角度）。
// 空を +θ 回す ＝ 見る向きを −θ 回してから、回す前の画像を読む。
float3 RotateSkyDir(float3 dir, float2 cs)
{
    return float3(cs.x * dir.x - cs.y * dir.z, dir.y, cs.y * dir.x + cs.x * dir.z);
}

// HDR の色 → 画面の色。
//   HDR の画像は「光の量」をそのまま持っていて、上限が無い（雲は 1 前後、太陽は数万）。
//   1. 露出（exposure）を掛ける。Environment の「空の明るさ」
//   2. トーンマップ（ACES の近似式）：暗いところはほぼそのまま、明るいところはなめらかに 1 へ寄せる
//   3. ガンマ補正：光の量 → 画面の明るさ（約 1/2.2 乗）。このエンジンの他の色（テクスチャなど）は
//      画面の明るさのまま計算しているので、それに合わせる
float3 HdrToScreen(float3 hdr, float exposure)
{
    float3 x = max(hdr, 0.0) * exposure;
    float3 t = saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
    return pow(t, 1.0 / 2.2);
}

#endif // SKY_COLOR_HLSLI