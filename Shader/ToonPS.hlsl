#include "Pbr.hlsli"      // VS_OUT + CbScene
#include "Material.hlsli" // CbMesh, texture / sampler slots
#include "Brdf.hlsli"     // PI + Cook-Torrance terms
#include "Shadow.hlsli"   // ShadowMap (t8) + ComputeShadow
#include "Ibl.hlsli"      // SampleIrradiance / DiffuseIBL / SpecularIBL

float posterize(float value, float step_count)
{
    return floor(value / (1.0f / step_count)) * (1.0f / step_count);
}

float2 posterize(float2 value, float step_count)
{
    return float2(posterize(value.x, step_count), posterize(value.y, step_count));
}

float3 posterize(float3 value, float step_count)
{
    return float3(posterize(value.x, step_count), posterize(value.y, step_count), posterize(value.z, step_count));
}

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 albedo = baseColorFactor;
    if (hasBaseColorMap)
        albedo *= BaseColorMap.Sample(LinearSampler, posterize(pin.texcoord, 64.0f));

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

    // 2. Specular: PBR spec shaped into a hard toon highlight
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

    // 3. IBL ambient (toon-modulated)
    float3 kS_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD_ibl = (1.0f - kS_ibl) * (1.0f - metalness);

    float3 irradiance = SampleIrradiance(N);
    float3 diffuseIBL = DiffuseIBL(kD_ibl, irradiance, albedo);
    float3 R = reflect(-V, N);
    float3 specularIBL = SpecularIBL(kS_ibl, R, roughness, NdotV);

    float ao = 1.0f;
    if (hasOcclusionMap)
    {
        float occ = OcclusionMap.Sample(LinearSampler, pin.texcoord).r;
        ao = lerp(1.0f, occ, occlusionStrength);
    }

    float3 ambient = (diffuseIBL + specularIBL) * ao;

    // ---- Cast shadow: darken direct + ambient together (with a floor) ----
    float shadow = ComputeShadow(pin.worldPosition);
    float shade = lerp(shadowParams.x, 1.0f, shadow); // shadowParams.x = brightness inside shadow

    float3 color = lerp(ambient * 0.25f, (Lo + ambient), shade);

    // emissive is not affected by shadow
    if (hasEmissiveMap)
        color += EmissiveMap.Sample(LinearSampler, pin.texcoord).rgb * emissiveFactor;
    else
        color += emissiveFactor;

    return float4(color, albedo.a);
}