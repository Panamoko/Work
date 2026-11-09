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
// ハイライトは GGX（今のゲームエンジンで標準の式）で計算する。
//   ハイライト = D（向きのそろい方） × G（凹凸の影） × F（フレネル） / (4 × N・V × N・L)
// 「光の総量は roughness によらず同じ」になるので、なめらかな面は小さく強く、粗い面は大きく弱く光る。

static const float kPI = 3.14159265;

// 非金属の「正面から見たときの反射率」。プラスチック・木・布・肌など、ほとんどの物が 4% 前後
static const float3 kDielectricF0 = float3(0.04, 0.04, 0.04);

// 映り込みを粗い面でぼかすときの、空の平均の色
static const float3 kSkyAverage = (kSkyHorizon * 2.0 + kSkyZenith + kSkyGround) * 0.25;

// roughness → 式で使う α²。roughness が小さすぎるとハイライトが1点に縮んで見えなくなるので、下限を付ける
float RoughnessAlpha2(float rough)
{
    float a = max(rough, 0.05);
    a *= a;
    return a * a;
}

// D：表面の細かい凹凸のうち、ちょうど光を目に返す向きの面がどれだけあるか（GGX / Trowbridge-Reitz）
float DistributionGGX(float ndh, float a2)
{
    float q = ndh * ndh * (a2 - 1.0) + 1.0;
    return a2 / (kPI * q * q);
}

// G：凹凸が光や視線をさえぎる分（Smith / Schlick-GGX）。光や視線が浅いほど弱くなる
float GeometrySmith(float ndv, float ndl, float rough)
{
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    return (ndv / (ndv * (1.0 - k) + k)) * (ndl / (ndl * (1.0 - k) + k));
}

// F：フレネル（Schlick の近似）。正面では f0、斜めから見るほど白（反射 100%）に近づく
float3 FresnelSchlick(float3 f0, float cosTheta)
{
    return f0 + (1.0 - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

// ---- 映り込み（IBL：周りの景色を映す） ----
// 景色があるとき（params.w = 1）は、事前に計算した2つを使う（UE などと同じ「スプリットサム」の方法）：
//   envSpecular … 空を粗さごとにぼかしたキューブマップ。粗いほど小さいミップ（ぼけた景色）を読む
//   brdfLut     … 映り込みの強さ = f0 × A ＋ B の A と B の表。視線が浅いほど B が増えて白く光る（フレネル）
// 景色が無いときは、前と同じグラデーションの空と f0 で計算する。
bool HasEnvironment()
{
    return params.w > 0.5;
}

// 向き r に映って見える景色の色（画面の色。粗いほどぼける）
float3 EnvironmentColor(float3 r, float rough)
{
    if (HasEnvironment())
    {
        uint w, h, mips;
        envSpecular.GetDimensions(0, w, h, mips);
        float lod = rough * (float) (mips - 1);
        return HdrToScreen(envSpecular.SampleLevel(envSamp, RotateSkyDir(r, sky.yz), lod).rgb, sky.x);
    }
    return lerp(SkyColor(r), kSkyAverage, rough);
}

// 映り込みの強さ（色ごと。0〜1）
float3 EnvironmentBrdf(float3 f0, float ndv, float rough)
{
    if (HasEnvironment())
    {
        float2 ab = brdfLut.SampleLevel(envSamp, float2(ndv, rough), 0).rg;
        return f0 * ab.x + ab.y;
    }
    return f0;
}

// 明るさを steps 段に丸める（色合いは保つ）。Toon の映り込み用
float3 PosterizeBrightness(float3 c, float steps)
{
    float v = max(c.r, max(c.g, c.b));
    if (v < 1e-4)
        return c;
    float q = max(floor(v * steps + 0.5), 1.0) / steps;
    return c * (q / v);
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
    float3 h = normalize(l + v); // 光と視線のちょうど間の向き
    float3 r = reflect(-v, n); // 鏡に映って見える向き

    // 材質。金属は自分の下地の色（拡散）を持たず、見えるのはハイライトと映り込み（どちらも下地の色が付く）
    float metallic = saturate(shading.z);
    float rough = saturate(specRim.y);
    float3 diffuseCol = base * (1.0 - metallic);

    // ハイライトの強さ。非金属は「ハイライトの強さ」（1 で現実の強さ）、金属は必ず 1（金属はハイライトそのものが見た目なので）
    float specScale = lerp(specRim.x, 1.0, metallic);

    // Lit と Toon で共通に使う値
    float ndl = dot(n, l);
    float ndh = saturate(dot(n, h));
    float a2 = RoughnessAlpha2(rough);
    float sh = (params.z > 0.5 && shading.y > 0.5) ? ShadowFactor(i.wpos) : 1.0;
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

        // スペキュラ：GGX の形（真ん中を 1 とした割合）が半分より明るい所を、くっきり塗る。
        //   大きさは roughness で決まる。粗い面は広がるぶん暗くする（roughness 0.5 以下は最大の明るさ）
        float q = ndh * ndh * (a2 - 1.0) + 1.0;
        float shape = saturate(a2 * a2 / (q * q));
        float spread = saturate(0.0625 / a2); // 0.0625 = roughness 0.5 のときの α²
        float3 specCol = lerp(float3(1.0, 1.0, 1.0), base, metallic);
        c += lightColor.rgb * specCol * (ToonStep(shape, 0.5, w) * specScale * spread * lit);

        c += rimColor.rgb * (ToonStep(rimRaw, 0.5, w) * specRim.z);

        // 金属の映り込み（Toon 風）：景色を大きくぼかしてから、明るさを3段に丸める（塗り分けたような見た目）
        float3 env = PosterizeBrightness(EnvironmentColor(r, max(rough, 0.5)), 3.0);
        c += base * env * metallic;
    }
    else
    {
        // ---- Lit：環境光 ＋ 平行光 × 影 ----
        float d = saturate(ndl);
        c = diffuseCol * (params.x + d * params.y * sh) * lightColor.rgb;

        // スペキュラ（GGX）。反射率 f0 は、非金属は 4%、金属は下地の色そのもの
        float ndv = max(dot(n, v), 1e-4);
        float3 f0 = lerp(kDielectricF0, base, metallic);
        float3 F = FresnelSchlick(f0, saturate(dot(v, h)));
        float3 spec = DistributionGGX(ndh, a2) * GeometrySmith(ndv, d, rough) * F / (4.0 * ndv * max(d, 1e-4));
        // このエンジンの拡散光は π で割っていない（下地の色 × cosθ）ので、ハイライトも π 倍して釣り合わせる
        c += lightColor.rgb * spec * (kPI * d * sh * params.y * specScale);

        // リムは視線だけで決まるので、影の中にも出る
        c += rimColor.rgb * (rimRaw * specRim.z);

        // 映り込み：鏡の向き r に見える景色 × 映り込みの強さ。
        //   金属は下地の色が付いてはっきり映る。非金属も、斜めから見るとうっすら映る（フレネル）
        c += EnvironmentColor(r, rough) * EnvironmentBrdf(f0, ndv, rough) * specScale;
    }
    return c;
}

#endif // LIGHTING_HLSLI
