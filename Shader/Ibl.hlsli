#ifndef IBL_HLSLI
#define IBL_HLSLI

// Image-based lighting helpers (split-sum approximation).
// Requires the IBL texture slots declared in Material.hlsli.

#include "Material.hlsli" // IrradianceMap, PrefilteredMap, BrdfLut, LinearClampSampler
#include "Brdf.hlsli"     // FresnelSchlickRoughness (used by callers)

// Diffuse irradiance sampled from the pre-integrated cube map.
float3 SampleIrradiance(float3 N)
{
    return IrradianceMap.Sample(LinearClampSampler, N).rgb;
}

// kD: diffuse weight, albedo: surface color.
float3 DiffuseIBL(float3 kD, float3 irradiance, float3 albedo)
{
    return kD * irradiance * albedo;
}

// Split-sum specular IBL. F is the (roughness-aware) Fresnel term.
// specular_pmrem.dds has mips 0..10, mapped by roughness * 10.
float3 SpecularIBL(float3 F, float3 reflectDir, float roughness, float NdotV)
{
    float3 prefiltered = PrefilteredMap.SampleLevel(LinearClampSampler, reflectDir, roughness * 10.0f).rgb;
    float2 envBrdf = BrdfLut.Sample(LinearClampSampler, float2(NdotV, roughness)).rg;
    return prefiltered * (F * envBrdf.x + envBrdf.y);
}

#endif // IBL_HLSLI