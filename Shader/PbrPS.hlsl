#include "Pbr.hlsli"      // VS_OUT + CbScene
#include "Material.hlsli" // CbMesh, texture / sampler slots
#include "Brdf.hlsli"     // PI + Cook-Torrance terms
#include "Ibl.hlsli"      // SampleIrradiance / DiffuseIBL / SpecularIBL

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

    // Direct lighting (Cook-Torrance).
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

    // IBL ambient.
    float3 kS_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD_ibl = (1.0f - kS_ibl) * (1.0f - metalness);
    float3 irradiance = SampleIrradiance(N);
    float3 diffuseIBL = DiffuseIBL(kD_ibl, irradiance, albedo);
    float3 R = reflect(-V, N);
    float3 specularIBL = SpecularIBL(kS_ibl, R, roughness, NdotV);
    float3 ambient = (diffuseIBL + specularIBL) * ao;

    float3 emissive = emissiveFactor;
    if (hasEmissiveMap)
        emissive *= SRGBToLinear(EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb);

    float3 color = ambient + Lo + emissive;
    color = color / (color + 1.0f);             // Reinhard tone map
    color = pow(max(color, 0.0f), 1.0f / 2.2f); // gamma

    return float4(color, baseColor.a);
}
