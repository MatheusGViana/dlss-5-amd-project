// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2026 Advanced Micro Devices, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "common.hlsl"

ConstantBuffer<TraceRaysConstants> Constants : register(b0);
ConstantBuffer<SceneInformation> SceneInfo : register(b1);
ConstantBuffer<SceneLightingInformation> LightInfo : register(b2);

RaytracingAccelerationStructure g_TLAS : register(t0);
Texture2D<float> g_Depthbuffer : register(t1);

Texture2D<float4> g_BrdfTexture : register(t2);
TextureCube g_PrefilteredCube : register(t3);
TextureCube g_IrradianceCube : register(t4);

#define DECLARE_SRV_REGISTER(idx)     t##idx
#define DECLARE_UAV_REGISTER(idx)     u##idx
#define DECLARE_SAMPLER_REGISTER(idx) s##idx

#define DECLARE_SRV(slot) register(DECLARE_SRV_REGISTER(slot))
#define DECLARE_UAV(slot) register(DECLARE_UAV_REGISTER(slot))
#define DECLARE_SAMPLER(slot) register(DECLARE_SAMPLER_REGISTER(slot))

StructuredBuffer<PTMaterialInfo> g_MaterialInfo : DECLARE_SRV(RAYTRACING_INFO_MATERIAL);
StructuredBuffer<PTInstanceInfo> g_InstanceInfo : DECLARE_SRV(RAYTRACING_INFO_INSTANCE);
StructuredBuffer<uint> g_SurfaceId : DECLARE_SRV(RAYTRACING_INFO_SURFACE_ID);
StructuredBuffer<PTSurfaceInfo> g_SurfaceInfo : DECLARE_SRV(RAYTRACING_INFO_SURFACE);
StructuredBuffer<uint> g_IndexBuffers[MAX_BUFFER_COUNT] : DECLARE_SRV(INDEX_BUFFER_BEGIN_SLOT);
StructuredBuffer<float> g_VertexBuffers[MAX_BUFFER_COUNT] : DECLARE_SRV(VERTEX_BUFFER_BEGIN_SLOT);

Texture2D g_ShadowMapTextures[MAX_SHADOW_MAP_TEXTURES_COUNT] : DECLARE_SRV(SHADOW_MAP_BEGIN_SLOT);
Texture2D g_Textures[MAX_TEXTURES_COUNT] : DECLARE_SRV(TEXTURE_BEGIN_SLOT);

SamplerState g_SamplerBRDF : register(s0);
SamplerState g_SamplerIrradianceCube : register(s1);
SamplerState g_SamplerPrefilteredCube : register(s2);
SamplerComparisonState SamShadow : register(s3); // needs to be named this to function in PBRLighting in lightingcommon.hlsl
SamplerState g_Samplers[MAX_SAMPLERS_COUNT] : DECLARE_SAMPLER(SAMPLER_BEGIN_SLOT);

RWTexture2D<float4> g_DirectSpecularTarget : register(u0);
RWTexture2D<float4> g_DirectDiffuseTarget : register(u1);
RWTexture2D<float4> g_IndirectSpecularTarget : register(u2);
RWTexture2D<float4> g_IndirectDiffuseTarget : register(u3);
RWTexture2D<float> g_DominantLightVisibilityTarget : register(u4);
RWTexture2D<float> g_AmbientOcclusionTarget : register(u5);
RWTexture2D<float> g_SpecularOcclusionTarget : register(u6);
RWTexture2D<float3> g_DiffuseAlbedoTarget : register(u7);
RWTexture2D<float3> g_SpecularAlbedoTarget : register(u8);
RWTexture2D<float4> g_NormalsTarget : register(u9);
RWTexture2D<float4> g_SkipTarget : register(u10);

#define brdfTexture g_BrdfTexture
float4 SampleBRDFTexture(float2 uv)
{
    return g_BrdfTexture.SampleLevel(g_SamplerBRDF, uv, 0);
}

#define irradianceCube g_IrradianceCube
float4 SampleIrradianceCube(float3 n)
{
    return g_IrradianceCube.SampleLevel(g_SamplerIrradianceCube, n, 0);
}

#define prefilteredCube g_PrefilteredCube
float4 SamplePrefilteredCube(float3 reflection, float lod)
{
    return g_PrefilteredCube.SampleLevel(g_SamplerPrefilteredCube, reflection, lod);
}

#define IBL_INDEX b3
#include "lightingcommon.h"

#include "raytracing_common.hlsl"

LightingResult CalculateLighting(inout uint rngState, out float dominantLightVisibility, float3 worldPosition, float3 normal, float3 view, MaterialInfo materialInfo, uint recursionIndex)
{
    // Accumulate contribution from punctual lights
    LightingResult totalResult = (LightingResult)0;
    for (int i = 0; i < LightInfo.LightCount; ++i)
    {
        LightInformation lightInfo = LightInfo.LightInfo[i];
        if (Constants.use_dominant_light && recursionIndex == 0 && i == Constants.dominant_light_index)
        {
            dominantLightVisibility = GetDominantLightHitDistance(g_TLAS, lightInfo, rngState, worldPosition, normal);
        }
        else
        {
            const float shadowFactor = GetShadowFactor(g_TLAS, lightInfo, rngState, worldPosition, normal, recursionIndex == 0);
            if (shadowFactor > 0.0f)
            {
                LightingResult lightResult;
                if (recursionIndex > 0)
                {
                    // Apply fully albedo modulated shading
                    lightResult = ApplyPunctualLight(worldPosition, normal, view, materialInfo, lightInfo);
                }
                else
                {
                    // Apply only lighting contribution
                    lightResult = ApplyLight(worldPosition, normal, view, materialInfo, lightInfo);
                }
                totalResult.diffuse  += lightResult.diffuse  * shadowFactor;
                totalResult.specular += lightResult.specular * shadowFactor;
            }
        }
    }

    return totalResult;
}

void TraceRay_OnHit(inout TraceRayHitResult result, in TraceRayDesc ray, in TraceRayHitInfo hitInfo, inout uint rngState)
{
    PTInstanceInfo instanceInfo = g_InstanceInfo.Load(hitInfo.instanceId);

    uint surfaceId = g_SurfaceId.Load((instanceInfo.surface_id_table_offset + hitInfo.geometryIndex));
    PTSurfaceInfo surfaceInfo = g_SurfaceInfo.Load(surfaceId);

    uint3 faceIndices;
    if (surfaceInfo.index_type == SURFACE_INFO_INDEX_TYPE_U16)
    {
        faceIndices = FetchFaceIndicesU16(g_IndexBuffers, surfaceInfo.index_offset, hitInfo.triangleId);
    }
    else // SURFACE_INFO_INDEX_TYPE_U32
    {
        faceIndices = FetchFaceIndicesU32(g_IndexBuffers, surfaceInfo.index_offset, hitInfo.triangleId);
    }

    LocalBasis localBasis = FetchLocalBasis(g_VertexBuffers, surfaceInfo, hitInfo.objectToWorld, faceIndices, hitInfo.barycentrics, hitInfo.isFrontFace);

    PTMaterialInfo material = g_MaterialInfo.Load(surfaceInfo.material_id);

    float mipLevel = CalculateMipLevelFromRayHit(hitInfo.rayT, localBasis);
    PBRPixelInfo pbrPixelInfo = GetPBRPixelInfoFromMaterial(g_Textures, g_Samplers, localBasis, material, mipLevel);
    pbrPixelInfo.pixelWorldPos = float4(hitInfo.position, 1.0f);
    pbrPixelInfo.pixelCoordinates = float4(0, 0, 0, 0);

    MaterialInfo materialInfo = GetMaterialInfo(pbrPixelInfo);
    float dominantLightVisibility = 0.0f;
    LightingResult lightingResult = CalculateLighting(rngState, dominantLightVisibility, pbrPixelInfo.pixelWorldPos.xyz, pbrPixelInfo.pixelNormal.xyz, -ray.direction, materialInfo, ray.recursionIndex);

    float3 emission = float3(0, 0, 0);
    //if (Constants.use_emission)
    {
        emission = float3(material.emission_factor_x, material.emission_factor_y, material.emission_factor_z) /* * Constants.emissive_factor*/;
        if (material.emission_tex_id >= 0)
        {
            emission = emission * g_Textures[NonUniformResourceIndex(material.emission_tex_id)].SampleLevel(g_Samplers[material.emission_tex_sampler_id], localBasis.uv, 0).xyz;
        }
    }

    result.radiance = lightingResult.diffuse + lightingResult.specular + emission;
    result.diffuseRadiance = lightingResult.diffuse;
    result.specularRadiance = lightingResult.specular;
    result.emission = emission;
    result.materialInfo = materialInfo;
    result.localBasis = localBasis;
    result.worldPosition = pbrPixelInfo.pixelWorldPos.xyz;
    result.worldNormal = pbrPixelInfo.pixelNormal.xyz;
    result.dominantLightVisibility = dominantLightVisibility;
}

void TraceRay_OnMiss(inout TraceRayHitResult result, in TraceRayDesc ray, inout uint rngState)
{
    // Sample radiance cache here...
    result.radiance = SamplePrefilteredCube(ray.direction, 0.0f).xyz * Constants.ibl_factor;
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    const uint2 pixel = dtid.xy;

    // Checkerboard: determine which pixels have data this frame
    // checkerboardOrigin 0 = pixel (0,0) is traced, 1 = pixel (1,0) is traced
    const uint checkerboardOrigin  = Constants.frame_index & 1u;
    const bool isCheckerboard      = Constants.checkerboard != 0u;
    const bool hasCheckerboardData = !isCheckerboard || (((pixel.x ^ pixel.y) & 1u) == checkerboardOrigin);
     // Checkerboard: radiance and occlusion outputs are packed to the left half of the texture.
    // The x coordinate is halved so only valid pixels are stored contiguously.
    const uint2 checkerboardPixel = isCheckerboard ? uint2(pixel.x >> 1u, pixel.y) : pixel;

    float z = g_Depthbuffer.Load(int3(pixel.xy, 0));
    float3 screenUVW = float3((float2(pixel) + float2(0.5f, 0.5f)) * Constants.inv_render_size, z);
    float3 pixelPosition = ScreenSpaceToWorldSpace(float3(screenUVW.xy, 1.0f), Constants.clip_to_world);
    const float3 cameraPosition = ViewSpaceToWorldSpace(float4(0.0f, 0.0f, 0.0f, 1.0f), Constants.camera_to_world);
    const float3 toCameraDirection = normalize(cameraPosition - pixelPosition);

    uint rngState = GenerateRngState(dtid.y, dtid.x, Constants.frame_index + 1);

    // Trace primary ray
    TraceRayDesc primaryRay;
    primaryRay.origin         = cameraPosition;
    primaryRay.direction      = -toCameraDirection;
    primaryRay.tMin           = 0.01f;
    primaryRay.tMax           = 1024.0f;
    primaryRay.recursionIndex = 0u;

    const TraceRayHitResult primaryHit = TraceRay(g_TLAS, primaryRay, rngState);
    if (primaryHit.hit)
    {
        MaterialInfo material = primaryHit.materialInfo;
        float roughness = material.perceptualRoughness;

        const float3x3 localToWorld = CreateTBN(primaryHit.worldNormal);

        float3 indirectSpecularRadiance    = 0.0f;
        float  indirectSpecularHitDistance = 65504.0f;
        float3 indirectDiffuseRadiance     = 0.0f;
        float  indirectDiffuseHitDistance  = 65504.0f;
        float  dominantLightVisibility     = 0.0f;

        // Only trace secondary rays for pixels that have checkerboard data
        if (hasCheckerboardData)
        {
            {
                // Trace indirect specular ray
                float pdf;
                const float3 direction = SampleSpecularReflectionVector(rngState, localToWorld, toCameraDirection, roughness, pdf);
                const float sampleWeight = 0.5f / max(pdf, EPSILON);

                TraceRayDesc secondaryRay   = primaryRay;
                secondaryRay.origin         = primaryHit.worldPosition + primaryHit.worldNormal * EPSILON;
                secondaryRay.direction      = direction;
                secondaryRay.recursionIndex = 1u;

                const TraceRayHitResult secondaryHit = TraceRay(g_TLAS, secondaryRay, rngState);
                indirectSpecularRadiance = sampleWeight * secondaryHit.radiance;
                if (secondaryHit.hit)
                {
                    indirectSpecularHitDistance = length(secondaryRay.origin - secondaryHit.worldPosition);
                }
            }

            {
                // Trace indirect diffuse ray
                float pdf;
                const float3 direction = SampleDiffuseReflectionVector(rngState, localToWorld, pdf);
                const float sampleWeight = 0.5f / max(pdf, EPSILON);

                TraceRayDesc secondaryRay   = primaryRay;
                secondaryRay.origin         = primaryHit.worldPosition + primaryHit.worldNormal * EPSILON;
                secondaryRay.direction      = direction;
                secondaryRay.recursionIndex = 1u;

                const TraceRayHitResult secondaryHit = TraceRay(g_TLAS, secondaryRay, rngState);
                indirectDiffuseRadiance = sampleWeight * secondaryHit.radiance;
                if (secondaryHit.hit)
                {
                    indirectDiffuseHitDistance = length(secondaryRay.origin - secondaryHit.worldPosition);
                }
            }
            
            dominantLightVisibility = primaryHit.dominantLightVisibility;
        }

        const float roll = RandomFloat01(rngState);

        // From getIBLContribution() in lightingcommon.h
        // In Vulkan and DirectX (0, 0) is on the top left corner. We need to flip the y-axis as the brdf lut has roughness y-up.
        const float NoV = dot(primaryHit.worldNormal, toCameraDirection);
        const float2 brdfSamplePoint = clamp(float2(NoV, 1.0 - material.perceptualRoughness), float2(0.0, 0.0), float2(1.0, 1.0));
        // retrieve a scale and bias to F0. See [1], Figure 3
        const float2 brdf = SampleBRDFTexture(brdfSamplePoint).rg;

        const float3 specularAlbedo = abs((material.reflectance0 * brdf.x + brdf.y) + lerp(-1.0f, 1.0f, roll) / 1024.0f); // RGB10A2_UNORM
        const float3 diffuseAlbedo  = material.baseColor; // Cauldron's base color = diffuse albedo

        // G-Buffer outputs: always written at full resolution
        g_DiffuseAlbedoTarget[pixel]  = sqrt(diffuseAlbedo);
        g_SpecularAlbedoTarget[pixel] = sqrt(specularAlbedo);
        g_NormalsTarget[pixel]        = float4(NormalToOctahedronUv(primaryHit.worldNormal), roughness, 0.0f);
        g_SkipTarget[pixel]           = float4(primaryHit.emission, 0.0f);

        // Radiance and occlusion outputs: written at half-width when checkerboarding, only for valid pixels
        if (hasCheckerboardData)
        {
            g_DirectSpecularTarget[checkerboardPixel]   = float4(primaryHit.specularRadiance, 0.0f);
            g_DirectDiffuseTarget[checkerboardPixel]    = float4(primaryHit.diffuseRadiance, 0.0f);
            g_IndirectSpecularTarget[checkerboardPixel] = float4(indirectSpecularRadiance, indirectSpecularHitDistance);
            g_IndirectDiffuseTarget[checkerboardPixel]  = float4(indirectDiffuseRadiance, indirectDiffuseHitDistance);
            
            // Dominant light visibility is in [0,FP16_MAX]
            g_DominantLightVisibilityTarget[checkerboardPixel] = dominantLightVisibility;
            
            const float primaryHitDistance = length(primaryRay.origin - primaryHit.worldPosition);
            // Ambient occlusion is in [0,1]
            g_AmbientOcclusionTarget[checkerboardPixel]  = saturate(indirectDiffuseHitDistance  / (Constants.ambientOcclusionA  + primaryHitDistance * Constants.ambientOcclusionB));
            // Specular occlusion is in [0,1]
            g_SpecularOcclusionTarget[checkerboardPixel] = saturate(indirectSpecularHitDistance / (Constants.specularOcclusionA + primaryHitDistance * Constants.specularOcclusionB));
        }
    }
    else
    {
        g_DiffuseAlbedoTarget[pixel]  = 0.0f;
        g_SpecularAlbedoTarget[pixel] = 0.0f;
        g_NormalsTarget[pixel]        = 0.0f;
        g_SkipTarget[pixel]           = float4(primaryHit.radiance, 0.0f);

        if (hasCheckerboardData)
        {
            g_DirectSpecularTarget[checkerboardPixel]          = 0.0f;
            g_DirectDiffuseTarget[checkerboardPixel]           = 0.0f;
            g_IndirectSpecularTarget[checkerboardPixel]        = float4(0.0f, 0.0f, 0.0f, 65504.0f);
            g_IndirectDiffuseTarget[checkerboardPixel]         = float4(0.0f, 0.0f, 0.0f, 65504.0f);
            g_DominantLightVisibilityTarget[checkerboardPixel] = 0.0f;
            g_AmbientOcclusionTarget[checkerboardPixel]        = 1.0f;
            g_SpecularOcclusionTarget[checkerboardPixel]       = 1.0f;
        }
    }
}
