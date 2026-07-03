#ifndef MATERIAL_HLSLI
#define MATERIAL_HLSLI

// Per-mesh material constants and the standard PBR texture / sampler slots.
// Shared by every surface shader (PBR, Toon, ...).

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

#endif // MATERIAL_HLSLI