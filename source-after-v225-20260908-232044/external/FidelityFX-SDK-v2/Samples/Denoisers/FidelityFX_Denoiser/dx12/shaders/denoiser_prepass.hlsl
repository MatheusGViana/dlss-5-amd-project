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

#include "commonintersect.hlsl"

Texture2D<float> g_DepthTarget : register(t0);

RWTexture2D<float> g_LinearDepth : register(u0);

RWTexture2D<float4> g_DenoisedDirectDiffuse           : register(u1);
RWTexture2D<float4> g_DenoisedDirectSpecular          : register(u2);
RWTexture2D<float4> g_DenoisedIndirectDiffuse         : register(u3);
RWTexture2D<float4> g_DenoisedIndirectSpecular        : register(u4);
RWTexture2D<float>  g_DenoisedDominantLightVisibility : register(u5);
RWTexture2D<float>  g_DenoisedAmbientOcclusion        : register(u6);
RWTexture2D<float>  g_DenoisedSpecularOcclusion       : register(u7);

cbuffer Constants : register(b0)
{
    float4x4 g_ClipToCamera;
    float g_RenderWidth;
    float g_RenderHeight;
    float2 g_padding;
};

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    const uint2 pixel = dtid.xy;
    if ((g_RenderWidth <= pixel.x) || (g_RenderHeight <= pixel.y))
    {
        return;
    }
    
    const float  depth        = g_DepthTarget[pixel];
    const float3 screenUVW    = float3((float2(pixel) + 0.5f) / float2(g_RenderWidth, g_RenderHeight), depth);
    const float3 viewSpacePos = ScreenSpaceToViewSpace(screenUVW, g_ClipToCamera);

    // Signed linear depth
    g_LinearDepth[pixel] = viewSpacePos.z;
    
    // clear outputs to validate that history is kept by denoiser internally
    g_DenoisedDirectDiffuse[pixel]           = 0.0f;
    g_DenoisedDirectSpecular[pixel]          = 0.0f;
    g_DenoisedIndirectDiffuse[pixel]         = 0.0f;
    g_DenoisedIndirectSpecular[pixel]        = 0.0f;
    g_DenoisedDominantLightVisibility[pixel] = 0.0f;
    g_DenoisedAmbientOcclusion[pixel]        = 0.0f;
    g_DenoisedSpecularOcclusion[pixel]       = 0.0f;
}
