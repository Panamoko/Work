#ifndef BRDF_HLSLI
#define BRDF_HLSLI

// Cook-Torrance BRDF building blocks shared by PBR and Toon pixel shaders.

static const float PI = 3.14159265358979f;

// Normal distribution (Trowbridge-Reitz / GGX).
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-6f);
}

// Geometry term (Schlick-GGX, direct lighting k).
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

// Fresnel (Schlick).
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// Fresnel with roughness bias (used for ambient / IBL).
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 r = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (r - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

#endif // BRDF_HLSLI