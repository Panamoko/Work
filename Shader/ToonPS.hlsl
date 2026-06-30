#include "Pbr.hlsli"

cbuffer CbMesh : register(b0)
{
    float4 baseColorFactor;
    float3 emissiveFactor;
    float metalnessFactor;
    float roughnessFactor;
    float occlusionStrength;
    float alphaCutoff;
    int alphaMode;
    int hasBaseColorMap;
    int hasNormalMap;
    int hasMetalnessRoughnessMap;
    int hasEmissiveMap;
    int hasOcclusionMap;
    int _pad0;
    int _pad1;
    int _pad2;
};

Texture2D BaseColorMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D MetalnessRoughnessMap : register(t2);
Texture2D EmissiveMap : register(t3);
Texture2D OcclusionMap : register(t4);
TextureCube IrradianceMap : register(t5);
TextureCube PrefilteredMap : register(t6);
Texture2D BrdfLut : register(t7);
Texture2D ShadowMap : register(t8);

SamplerState LinearSampler : register(s0);
SamplerState LinearClampSampler : register(s1);
SamplerComparisonState ShadowSampler : register(s2);

static const float PI = 3.14159265358979f;

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = (NdotH * NdotH * (a2 - 1.0f) + 1.0f);
    return a2 / (PI * d * d);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

// 0 = fully shadowed, 1 = fully lit (PCF 3x3)
float ComputeShadow(float3 worldPos)
{
    if (shadowParams.z < 0.5f)
        return 1.0f;

    float4 lp = mul(float4(worldPos, 1.0f), lightViewProjection);
    lp.xyz /= lp.w;
    if (lp.z > 1.0f || lp.z < 0.0f)
        return 1.0f;

    float2 uv = lp.xy * float2(0.5f, -0.5f) + 0.5f;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    float depth = lp.z - shadowParams.y;
    float texel = shadowParams.w;

    float s = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; y++)
        [unroll]
        for (int x = -1; x <= 1; x++)
            s += ShadowMap.SampleCmpLevelZero(ShadowSampler, uv + float2(x, y) * texel, depth);
    return s / 9.0f;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 albedo = baseColorFactor;
    if (hasBaseColorMap)
        albedo *= BaseColorMap.Sample(LinearSampler, pin.texcoord);

    if (alphaMode == 1 && albedo.a < alphaCutoff)
        discard;

    float metalness = metalnessFactor;
    float roughness = roughnessFactor;
    if (hasMetalnessRoughnessMap)
    {
        float4 mr = MetalnessRoughnessMap.Sample(LinearSampler, pin.texcoord);
        roughness *= mr.g;
        metalness *= mr.b;
    }

    float3 N = normalize(pin.normal);
    if (hasNormalMap)
    {
        float3 tangentNormal = NormalMap.Sample(LinearSampler, pin.texcoord).rgb * 2.0f - 1.0f;
        float3 T = normalize(pin.tangent.xyz);
        float3 B = normalize(cross(N, T)) * pin.tangent.w;
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(tangentNormal, TBN));
    }

    float3 V = normalize(cameraPosition.xyz - pin.worldPosition);
    float3 L = normalize(-lightDirection.xyz);
    float3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0001f);
    float NdotL = max(dot(N, L), 0.0001f);
    float NdotH = max(dot(N, H), 0.0f);
    float VdotH = max(dot(V, H), 0.0f);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metalness);

    // 1. Diffuse: soft toon ramp
    float halfLambert = saturate(dot(N, L) * 0.5f + 0.5f);
    float toonStep = smoothstep(0.42f, 0.58f, halfLambert);
    float blendFactor = 0.40f;
    float hybridDiffuse = lerp(toonStep, halfLambert, blendFactor);

    float diffuseShadow = 0.55f;
    float3 toonAlbedo = albedo.rgb * lerp(diffuseShadow, 1.0f, hybridDiffuse);

    // 2. Specular
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(VdotH, F0);
    float3 specularPBR = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);

    float specIntensity = length(specularPBR);
    float specThreshold = lerp(1.0f, 0.1f, 1.0f - roughness);
    float toonSpecular = smoothstep(specThreshold - 0.05f, specThreshold + 0.05f, specIntensity);
    float3 finalSpecular = F0 * toonSpecular;

    float3 kD = (1.0f - F) * (1.0f - metalness);
    float3 Lo = (kD * toonAlbedo / PI + finalSpecular) * lightColor.rgb * halfLambert;

    // 3. IBL ambient
    float3 kS_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD_ibl = (1.0f - kS_ibl) * (1.0f - metalness);

    float3 irradiance = IrradianceMap.Sample(LinearClampSampler, N).rgb;
    float irradIntensity = length(irradiance);
    float toonIrrad = smoothstep(0.4f, 0.6f, irradIntensity);
    float hybridIrrad = lerp(toonIrrad, irradIntensity, 0.2f);
    irradiance = lerp(irradiance * 0.5f, irradiance, hybridIrrad);
    float3 diffuseIBL = kD_ibl * irradiance * albedo.rgb;

    float3 R = reflect(-V, N);
    float3 prefiltered = PrefilteredMap.SampleLevel(LinearClampSampler, R, roughness * 10.0f).rgb;
    float2 envBRDF = BrdfLut.Sample(LinearClampSampler, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefiltered * (kS_ibl * envBRDF.x + envBRDF.y);
    float toonIblSpec = smoothstep(0.5f, 0.6f, length(specularIBL));
    specularIBL *= toonIblSpec;

    float ao = 1.0f;
    if (hasOcclusionMap)
    {
        float occ = OcclusionMap.Sample(LinearSampler, pin.texcoord).r;
        ao = lerp(1.0f, occ, occlusionStrength);
    }

    float3 ambient = (diffuseIBL + specularIBL) * ao;
    ambient += albedo.rgb * 0.12f; // ambient floor

    // ---- Cast shadow: darken direct + ambient together (with a floor) ----
    float shadow = ComputeShadow(pin.worldPosition);
    float shade = lerp(shadowParams.x, 1.0f, shadow); // shadowParams.x = brightness inside shadow

    float3 color = (Lo + ambient) * shade;

    // emissive is not affected by shadow
    if (hasEmissiveMap)
        color += EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb * emissiveFactor;
    else
        color += emissiveFactor;

    return float4(color, albedo.a);
}