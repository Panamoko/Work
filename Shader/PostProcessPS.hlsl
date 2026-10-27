#include "PostProcess.hlsli"

// ポストプロセス：シーンを描いた画像を1ピクセルずつ読んで、彩度と輪郭線を掛ける。

float RawDepth(float2 uv)
{
    return depthTex.SampleLevel(samp, uv, 0).r;
}

// 深度バッファの値 → カメラの前方向の距離（ワールドの長さ）。
// 深度バッファは直線的ではない（手前に値の幅の大半を使う）ので、射影を逆にたどって戻す。
float ToDistance(float rawDepth)
{
    return depth.y / (rawDepth - depth.x);
}

// しきい値より下は 0。しきい値の 1.5 倍で 1 になる（いきなり切り替えず、少しだけなだらかに）。
float OverThreshold(float value, float threshold)
{
    return saturate((value - threshold) / max(threshold * 0.5, 1e-5));
}

// ---- エッジ検出 ----
// 1ピクセルの「線の濃さ」（0 = 線なし 〜 1 = 線）を、周り4方向の隣と比べて決める。
//
// 深度：生の深度の「2階の差分」（向かい合う両隣の合計 − 中心の2倍）を使う。
//   生の深度の値は、平らな面の上ではどれだけ斜めから見ても一定の割合で変わるので、
//   床や壁の上ではちょうど 0 になり、奥行きが飛ぶ場所だけ大きくなる。
//   （距離をそのまま比べると、浅い角度で見た床一面に線が出てしまう）
// 法線：1 − dot(中心, 隣)。同じ向きなら 0、直角なら 1。
//   奥行きはつながっているが面の向きが変わる場所（壁と床の境目など）を見つける。
float EdgeAmount(float2 uv)
{
    float2 pixel = depth.zw * max(edgeParams.x, 0.5);

    float d0 = RawDepth(uv);
    float z = ToDistance(d0);
    float4 n0 = normalTex.SampleLevel(samp, uv, 0);
    float3 nc = n0.rgb * 2.0 - 1.0;

    static const float2 kDirs[4] = { float2(1.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0), float2(1.0, -1.0) };
    float depthEdge = 0.0;
    float normalEdge = 0.0;
    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        float2 o = kDirs[k] * pixel;

        depthEdge = max(depthEdge, abs(RawDepth(uv + o) + RawDepth(uv - o) - 2.0 * d0));

        float4 na = normalTex.SampleLevel(samp, uv + o, 0);
        float4 nb = normalTex.SampleLevel(samp, uv - o, 0);
        // a = 0 は空（法線が無い）。空との境目は深度のほうで見つける。
        if (n0.a > 0.5 && na.a > 0.5)
            normalEdge = max(normalEdge, 1.0 - dot(nc, na.rgb * 2.0 - 1.0));
        if (n0.a > 0.5 && nb.a > 0.5)
            normalEdge = max(normalEdge, 1.0 - dot(nc, nb.rgb * 2.0 - 1.0));
    }

    // 生の深度の差を「距離に対して、奥行きがどれだけ飛んだか（割合）」に直す。
    // こうすると、近くでも遠くでも同じしきい値で効く。
    float relativeJump = depthEdge * z / abs(depth.y);

    float e = max(OverThreshold(relativeJump, edgeParams.y), OverThreshold(normalEdge, edgeParams.z));

    // 遠くは線が密集してちらつくので、最大距離の手前から薄くして消す。
    e *= 1.0 - smoothstep(edgeParams.w * 0.7, edgeParams.w, z);
    return e;
}

float4 main(PostVSOut i) : SV_TARGET
{
    int view = (int) (grade.y + 0.5);

    // 確認用の表示：後処理が読んでいる元データをそのまま見せる
    if (view == 1)
        return float4(normalTex.SampleLevel(samp, i.uv, 0).rgb, 1.0);
    if (view == 2)
    {
        float g = saturate(ToDistance(RawDepth(i.uv)) / 50.0); // カメラの位置で黒、50m 以上で白
        return float4(g, g, g, 1.0);
    }
    if (view == 3)
    {
        float g = 1.0 - EdgeAmount(i.uv); // 検出した線だけ（白地に黒）
        return float4(g, g, g, 1.0);
    }

    float3 c = sceneTex.SampleLevel(samp, i.uv, 0).rgb;

    // 彩度：白黒にした色と元の色を混ぜる（0 で白黒、1 でそのまま、1 より大きいと鮮やか）
    float gray = dot(c, float3(0.299, 0.587, 0.114));
    c = lerp(float3(gray, gray, gray), c, grade.x);

    // 輪郭線：彩度の影響を受けないように最後に重ねる
    if (edgeColor.a > 0.5)
        c = lerp(c, edgeColor.rgb, EdgeAmount(i.uv));

    return float4(saturate(c), 1.0);
}
