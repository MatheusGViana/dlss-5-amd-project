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

#ifndef IMPORTANCE_SAMPLING_H
#define IMPORTANCE_SAMPLING_H

#include "common.hlsl"

float3 SampleCone(inout uint rngState, float3 direction, float cosHalfAngle)
{
    float z = RandomFloat01(rngState) * (1.0f - cosHalfAngle) + cosHalfAngle;
    float phi = RandomFloat01(rngState) * 2.0f * M_PI;

    float x = sqrt(1.0f - z * z) * cos(phi);
    float y = sqrt(1.0f - z * z) * sin(phi);
    float3 north = float3(0.0f, 0.0f, 1.0f);

    float3 axis = normalize(cross(north, direction));
    float angle = acos(dot(north, direction));

    float3x3 rotation = CreateRotation3x3(angle, axis);

    return mul(rotation, float3(x, y, z));
}

float2 ConcentricSampleDisk(inout uint rngState)
{
    float2 u = float2(RandomFloat01(rngState), RandomFloat01(rngState));
    float2 uOffset = 2.f * u - float2(1, 1);

    if (uOffset.x == 0 && uOffset.y == 0)
    {
        return float2(0, 0);
    }

    float theta, r;
    if (abs(uOffset.x) > abs(uOffset.y))
    {
        r = uOffset.x;
        theta = M_PI_OVER_FOUR * (uOffset.y / uOffset.x);
    }
    else
    {
        r = uOffset.y;
        theta = M_PI_OVER_TWO - M_PI_OVER_FOUR * (uOffset.x / uOffset.y);
    }
    return r * float2(cos(theta), sin(theta));
}

float D_GGX(float NeDotVe, float alphaRoughness)
{
    if (NeDotVe < 0.0f)
    {
        return 0.0f;
    }

    // Numerically stable form of Walter 2007 GGX NDF
    float eps = 1e-5f;
    float denom = (1.0f - NeDotVe * NeDotVe) / (alphaRoughness + eps) + NeDotVe * NeDotVe;
    return 1.0f / (M_PI * alphaRoughness * denom * denom);
}

float G_SmithJointGGX(float NdotL, float NdotV, float alphaRoughnessSq)
{
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGX = GGXV + GGXL;
    if (GGX > 0.0f)
    {
        return 0.5f / GGX;
    }
    return 0.0f;
}

//------------------------------------------------------------------------------
// Diffuse Reflections
//------------------------------------------------------------------------------
    
float3 SampleCosineWeightedHemisphere(inout uint rngState, out float pdf)
{
    float3 rLocal;
    rLocal.xy = ConcentricSampleDisk(rngState);
    rLocal.z  = sqrt(1.0f - saturate(dot(rLocal.xy, rLocal.xy)));
    pdf = rLocal.z * M_ONE_OVER_PI;
    return rLocal;
}
    
float3 SampleDiffuseReflectionVector(inout uint rngState, float3x3 localToWorld, out float pdf)
{
    const float3 rLocal = SampleCosineWeightedHemisphere(rngState, pdf);

    return normalize(mul(rLocal, localToWorld));
}
    
//------------------------------------------------------------------------------
// Specular Reflections
//------------------------------------------------------------------------------
    
float3 SampleVndfHemisphereBounded(inout uint rngState, float3 vLocal, float2 alpha, out float pdf)
{
    // [Source]
	// K. Eto, Y. Tokuyoshi: Bounded VNDF Sampling for Smith–GGX Reflections.
        
    const float3 vLocalStd = normalize(float3(vLocal.xy * alpha, vLocal.z));
    // Sample a spherical cap.
    const float phi = 2.0f * M_PI * RandomFloat01(rngState);
    const float a = saturate(min(alpha.x, alpha.y)); // Eq. 6
    const float s = 1.0f + length(vLocal.xy); // Omit sgn for a <=1
    const float a2 = a * a;
    const float s2 = s * s;
    const float k = (1.0f - a2) * s2 / (s2 + a2 * vLocal.z * vLocal.z); // Eq. 5
    const float b = (0.0f < vLocal.z) ? k * vLocalStd.z : vLocalStd.z;
    const float z = mad(1.0f - RandomFloat01(rngState), 1.0f + b, -b);
    const float sinTheta = sqrt(saturate(1.0f - z * z));
    const float3 rLocalStd = { sinTheta * cos(phi), sinTheta * sin(phi), z };
    // Compute the microfacet normal m.
    const float3 mLocalStd = vLocalStd + rLocalStd;
    const float3 mLocal = normalize(float3(mLocalStd.xy * alpha, mLocalStd.z));
    // Return the reflection vector r.
    const float3 rLocal = reflect(-vLocal, mLocal);
        
    const float ndf = D_GGX(mLocal.z, a);
    const float2 av = alpha * vLocal.xy;
    const float len2 = dot(av, av);
    const float t = sqrt(len2 + vLocal.z * vLocal.z);
    if (0.0f <= vLocal.z)
    {
        pdf = ndf / (2.0f * (k * vLocal.z + t)); // Eq . 8 * || dm / do ||
    }
    else
    {
        pdf = ndf * (t - vLocal.z) / (2.0f * len2); // Eq . 7 * || dm / do ||
    }
        
    return rLocal;
}
    
float3 SampleSpecularReflectionVector(inout uint rngState, float3x3 localToWorld, float3 vWorld, float roughness, out float pdf)
{
    const float3 nWorld = localToWorld[2];
            
    if (roughness < 0.1f)
    {
        // If roughness is extremely low, the surface is basically a
        // perfect mirror.
        pdf = 1.0f;
        return reflect(-vWorld, nWorld);
    }

    //const float3 vLocal = mul(vWorld, transpose(localToWorld));
    const float3 vLocal = mul(localToWorld, vWorld);
    const float alpha = roughness * roughness;
    const float3 rLocal = SampleVndfHemisphereBounded(rngState, vLocal, alpha.xx, pdf);

    return normalize(mul(rLocal, localToWorld));
}

#endif  // IMPORTANCE_SAMPLING_H
