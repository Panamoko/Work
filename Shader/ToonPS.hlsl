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

SamplerState LinearSampler : register(s0);
SamplerState LinearClampSampler : register(s1);

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
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 albedo = baseColorFactor;
    if (hasBaseColorMap)
    {
        float4 texColor = BaseColorMap.Sample(LinearSampler, pin.texcoord);
        albedo *= texColor;
    }

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

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo.rgb, metalness);

    // ==========================================
    // Hybrid look: PBR + Lambert + Toon
    // ==========================================

    // 1. Diffuse: soft toon ramp blended toward Lambert (no harsh edge, no black)
    float halfLambert = saturate(dot(N, L) * 0.5f + 0.5f);
    float toonStep = smoothstep(0.42f, 0.58f, halfLambert); // softened cel step
    float blendFactor = 0.40f; // 40% Lambert softness mixed in
    float hybridDiffuse = lerp(toonStep, halfLambert, blendFactor);

    float shadowStrength = 0.55f; // shadow brightness floor (higher = brighter shadows)
    float3 toonAlbedo = albedo.rgb * lerp(shadowStrength, 1.0f, hybridDiffuse);

    // 2. Specular (PBR Cook-Torrance, lightly toon-ized)
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

    // 3. IBL (PBR ambient: diffuse irradiance + specular reflection)
    float3 kS_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD_ibl = (1.0f - kS_ibl) * (1.0f - metalness);

    float3 irradiance = IrradianceMap.Sample(LinearClampSampler, N).rgb;
    float3 diffuseIBL = kD_ibl * irradiance * albedo.rgb;

    float3 R = reflect(-V, N);
    float3 prefiltered = PrefilteredMap.SampleLevel(LinearClampSampler, R, roughness * 10.0f).rgb;
    float2 envBRDF = BrdfLut.Sample(LinearClampSampler, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefiltered * (kS_ibl * envBRDF.x + envBRDF.y);

    float ao = 1.0f;
    if (hasOcclusionMap)
    {
        float occ = OcclusionMap.Sample(LinearSampler, pin.texcoord).r;
        ao = lerp(1.0f, occ, occlusionStrength);
    }

    float3 ambient = (diffuseIBL + specularIBL) * ao;

    // Ambient floor: guarantees a minimum brightness so nothing turns pure black.
    ambient += albedo.rgb * 0.12f;

    float3 color = Lo + ambient;

    // 4. Emissive
    if (hasEmissiveMap)
    {
        float3 emissive = EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb;
        color += emissive * emissiveFactor;
    }
    else
    {
        color += emissiveFactor;
    }

    // (Distance fog removed: distant areas no longer turn gray.)

    return float4(color, albedo.a);
}