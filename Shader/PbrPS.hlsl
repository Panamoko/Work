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
    float d = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-6f);
}

float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotX / (NdotX * (1.0f - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 r = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (r - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 SRGBToLinear(float3 c)
{
    return pow(max(c, 0.0f), 2.2f);
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 baseColor = baseColorFactor;
    if (hasBaseColorMap)
    {
        float4 t = BaseColorMap.Sample(LinearSampler, pin.texcoord);
        baseColor.rgb *= SRGBToLinear(t.rgb);
        baseColor.a *= t.a;
    }
    if (alphaMode == 1 && baseColor.a < alphaCutoff)
        discard;

    float3 albedo = baseColor.rgb;
    float metalness = metalnessFactor;
    float roughness = roughnessFactor;

    if (hasMetalnessRoughnessMap)
    {
        float4 mr = MetalnessRoughnessMap.Sample(LinearSampler, pin.texcoord);
        roughness *= mr.g;
        metalness *= mr.b;
    }
    roughness = clamp(roughness, 0.04f, 1.0f);

    float3 N = normalize(pin.normal);
    if (hasNormalMap)
    {
        float3 T = normalize(pin.tangent.xyz);
        float3 B = cross(N, T) * pin.tangent.w;
        float3 n = NormalMap.Sample(LinearSampler, pin.texcoord).rgb * 2.0f - 1.0f;
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(n, TBN));
    }

    float3 V = normalize(cameraPosition.xyz - pin.worldPosition);
    float3 L = normalize(-lightDirection.xyz);
    float3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 1e-4f);
    float NdotL = max(dot(N, L), 0.0f);
    float NdotH = max(dot(N, H), 0.0f);
    float VdotH = max(dot(V, H), 0.0f);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metalness);

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    float3 F = FresnelSchlick(VdotH, F0);
    float3 specular = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);
    float3 kD = (1.0f - F) * (1.0f - metalness);
    float3 Lo = (kD * albedo / PI + specular) * lightColor.rgb * NdotL;

    float ao = 1.0f;
    if (hasOcclusionMap)
    {
        float occ = OcclusionMap.Sample(LinearSampler, pin.texcoord).r;
        ao = lerp(1.0f, occ, occlusionStrength);
    }

    // IBL 環境光
    float3 kS_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD_ibl = (1.0f - kS_ibl) * (1.0f - metalness);
    float3 irradiance = IrradianceMap.Sample(LinearClampSampler, N).rgb;
    float3 diffuseIBL = kD_ibl * irradiance * albedo;

    float3 R = reflect(-V, N);
    // specular_pmrem.dds は mip 0〜10 の 11段（roughness * 10 でマッピング）
    float3 prefiltered = PrefilteredMap.SampleLevel(LinearClampSampler, R, roughness * 10.0f).rgb;
    float2 envBrdf = BrdfLut.Sample(LinearClampSampler, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefiltered * (F0 * envBrdf.x + envBrdf.y);

    float3 ambient = (diffuseIBL + specularIBL) * ao;

    float3 emissive = emissiveFactor;
    if (hasEmissiveMap)
        emissive *= SRGBToLinear(EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb);

    float3 color = ambient + Lo + emissive;
    color = color / (color + 1.0f);
    color = pow(max(color, 0.0f), 1.0f / 2.2f);

    return float4(PrefilteredMap.SampleLevel(LinearClampSampler, reflect(-V, N), 0).rgb, 1.0f);
}