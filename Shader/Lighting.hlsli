#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

#include "Common.hlsli"
#include "SkyColor.hlsli"

// メッシュの色の計算（ライティング・Toon・材質）。SurfacePS.hlsl から使う。

// ---- 影 ----
// ワールド位置 wpos が、影の中なら 0、日なたなら 1（3x3 PCF で縁をなめらかに）。
float ShadowFactor(float3 wpos)
{
    float4 lp = mul(float4(wpos, 1.0), lightViewProj);
    lp.xyz /= lp.w;
    float2 uv = lp.xy * float2(0.5, -0.5) + 0.5;
    // シャドウマップの範囲の外（横・奥行きとも）は「影なし」にする
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || lp.z < 0.0 || lp.z > 1.0)
        return 1.0;
    float depth = lp.z - 0.0025;
    float texel = 1.0 / 2048.0; // シャドウマップの大きさ（C++ 側の 2048 と合わせる）
    float sum = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    [unroll]
        for (int x = -1; x <= 1; ++x)
            sum += shadowMap.SampleCmpLevelZero(shadowSamp, uv + float2(x, y) * texel, depth);
    float shadow = sum / 9.0;

    // シャドウマップはカメラに追従する正方形。端に近づいたら影を薄くして、直線で切れないようにする
    float border = max(abs(lp.x), abs(lp.y)); // 正方形の中心で 0、端で 1
    return lerp(shadow, 1.0, smoothstep(0.85, 1.0, border));
}

// なめらかな 0〜1 の値を、edge を境に「ほぼ 0 か 1」にする。width は境目のぼかし幅（片側）。
float ToonStep(float value, float edge, float width)
{
    return smoothstep(edge - width, edge + width, value);
}

// ---- Toon 専用：テクスチャの単純化 ----
// 細かい（写真のような）テクスチャを、塗ったような見た目にする。
//   1. ならす：半径 toon.w（UV）の円周上の8点と平均して、細かい模様を消す
//   2. 段階化：明るさを toon.z 段に丸める。色相は保つので色がずれない
// どちらも値が 0 なら何もしない。
float3 SimplifyTexture(float2 uv, float3 texColor)
{
    float radius = toon.w;
    if (radius > 0.0)
    {
        static const float2 kOffsets[8] =
        {
            float2(1.0, 0.0), float2(-1.0, 0.0), float2(0.0, 1.0), float2(0.0, -1.0),
            float2(0.707, 0.707), float2(-0.707, 0.707), float2(0.707, -0.707), float2(-0.707, -0.707)
        };
        float3 sum = texColor;
        [unroll]
        for (int k = 0; k < 8; ++k)
            sum += tex.SampleLevel(samp, uv + kOffsets[k] * radius, 0).rgb;
        texColor = sum / 9.0;
    }

    float steps = toon.z;
    if (steps >= 1.5)
    {
        float v = max(texColor.r, max(texColor.g, texColor.b)); // 明るさ
        if (v > 1e-4)
        {
            float q = max(floor(v * steps + 0.5), 1.0) / steps; // 丸めた明るさ（真っ黒にはしない）
            texColor *= q / v;
        }
    }
    return texColor;
}

// ---- 材質：metallic / roughness ----
// 映り込みを粗い面でぼかすときの、空の平均の色
static const float3 kSkyAverage = (kSkyHorizon * 2.0 + kSkyZenith + kSkyGround) * 0.25;

// roughness から Blinn-Phong の指数を作る（0.5 でおよそ 30）。なめらかなほど小さく鋭いハイライトになる。
float SpecularPower(float rough)
{
    float a = max(rough * rough, 0.03);
    return clamp(2.0 / (a * a) - 2.0, 1.0, 2048.0);
}

// ---- 1ピクセルの色 ----
float3 ShadeSurface(PSIn i)
{
    // mode は C++ の ShadingMode（ShadingParams.h）と値を合わせる
    int mode = (int) (shading.x + 0.5);

    float4 t = tex.Sample(samp, i.uv);
    if (mode == 2)
        t.rgb = SimplifyTexture(i.uv, t.rgb);
    float3 base = t.rgb * i.col.rgb * color.rgb;

    if (mode == 1)
        return base; // Unlit：テクスチャ × 色 そのまま

    float3 n = normalize(i.nrm);
    float3 l = normalize(-lightDir.xyz); // 面 → 光
    float3 v = normalize(cameraPos.xyz - i.wpos); // 面 → カメラ
    float3 h = normalize(l + v);
    float3 r = reflect(-v, n); // 鏡に映って見える向き

    // 材質。金属は自分の下地の色（拡散）を持たず、見えるのはハイライトと映り込み（どちらも下地の色が付く）
    float metallic = saturate(shading.z);
    float rough = saturate(specRim.y);
    float3 diffuseCol = base * (1.0 - metallic);
    float3 specCol = lerp(float3(1.0, 1.0, 1.0), base, metallic);

    // Lit と Toon で共通に使う値（ndl 以外は 0〜1 のなめらかな値）
    float ndl = dot(n, l);
    float sh = (params.z > 0.5 && shading.y > 0.5) ? ShadowFactor(i.wpos) : 1.0;
    float specRaw = pow(saturate(dot(n, h)), SpecularPower(rough)); // Blinn-Phong
    float rimRaw = pow(1.0 - saturate(dot(n, v)), max(specRim.w, 0.01)); // 輪郭に近いほど明るい

    float3 c;
    if (mode == 2)
    {
        // ---- Toon：明るい面と陰の面の2色に分ける ----
        float w = max(toon.y, 0.001);
        float hl = ndl * 0.5 + 0.5; // ハーフランバート（0〜1）
        float lit = ToonStep(hl, toon.x, w) * ToonStep(sh, 0.5, w); // 落ちた影も「陰の面」にする

        // 一番明るいところは Lit と同じ（環境光＋光の強さ）。色が飛ばないよう 1 で止める
        float3 lightCol = lightColor.rgb * saturate(params.x + params.y);
        c = diffuseCol * lightCol * lerp(toonShade.rgb, float3(1.0, 1.0, 1.0), lit);

        // スペキュラとリムは、くっきりした形にする。大きさは roughness / リムの鋭さで決まる
        c += lightColor.rgb * specCol * (ToonStep(specRaw, 0.5, w) * specRim.x * lit);
        c += rimColor.rgb * (ToonStep(rimRaw, 0.5, w) * specRim.z);

        // 金属の映り込み（Toon 風）：上は空・下は地面の2色で、地平線の境目をくっきり出す。粗い面ほどぼける
        float up = ToonStep(r.y, 0.0, max(w, rough * 0.5));
        float3 env = lerp(kSkyGround, lerp(kSkyHorizon, kSkyZenith, 0.5), up);
        c += base * env * metallic;
    }
    else
    {
        // ---- Lit：環境光 ＋ 平行光 × 影 ----
        float d = saturate(ndl);
        c = diffuseCol * (params.x + d * params.y * sh) * lightColor.rgb;

        // スペキュラは上に足す。影の中と、光が当たらない面には出さない
        c += lightColor.rgb * specCol * (specRaw * specRim.x * smoothstep(0.0, 0.1, d) * sh * params.y);

        // リムは視線だけで決まるので、影の中にも出る
        c += rimColor.rgb * (rimRaw * specRim.z);

        // 金属の映り込み：鏡の向きに見える空の色。粗い面ほど空の平均の色に近づく
        float3 env = lerp(SkyColor(r), kSkyAverage, rough);
        c += base * env * metallic;
    }
    return c;
}

#endif // LIGHTING_HLSLI
