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

cbuffer Constants : register(b0)
{
    float2 input_debug_view_size;
    float2 output_target_size;
};

SamplerState g_linear_sampler : register(s0);
Texture2D<float4> g_input_debug_view : register(t0);
RWTexture2D<float4> g_output_target : register(u0);

[numthreads(8,8,1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    float2 uv = (dtid.xy + 0.5) / output_target_size;
    float4 debug_view = g_input_debug_view.SampleLevel(g_linear_sampler, uv, 0);
    float4 current_color = g_output_target[dtid.xy];
    g_output_target[dtid.xy] = float4((debug_view.xyz * debug_view.w) + (current_color.xyz * (1 - debug_view.w)), current_color.w);
}