/*=============================================================================
                                                           
 d8b 888b     d888 888b     d888 8888888888 8888888b.   .d8888b.  8888888888 
 Y8P 8888b   d8888 8888b   d8888 888        888   Y88b d88P  Y88b 888        
     88888b.d88888 88888b.d88888 888        888    888 Y88b.      888        
 888 888Y88888P888 888Y88888P888 8888888    888   d88P  "Y888b.   8888888    
 888 888 Y888P 888 888 Y888P 888 888        8888888P"      "Y88b. 888        
 888 888  Y8P  888 888  Y8P  888 888        888 T88b         "888 888        
 888 888   "   888 888   "   888 888        888  T88b  Y88b  d88P 888        
 888 888       888 888       888 8888888888 888   T88b  "Y8888P"  8888888888                                                                 
                                                                            
    Copyright (c) Pascal Gilcher. All rights reserved.
    
    * Unauthorized copying of this file, via any medium is strictly prohibited
 	* Proprietary and confidential

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.

===============================================================================

    Author:         Pascal Gilcher

    More info:      https://martysmods.com
                    https://patreon.com/mcflypg
                    https://github.com/martymcmodding  	

=============================================================================*/



Texture2D<float4> ColorTex : register(t0);
Texture2D<float> DepthTex : register(t1);
SamplerState ClampLinear : register(s0);
RWTexture2D<float> stNEWGI_Z0 : register(u0);
RWTexture2D<float> stNEWGI_Z1 : register(u1);
RWTexture2D<float> stNEWGI_Z2 : register(u2);
RWTexture2D<float> stNEWGI_Z3 : register(u3);
RWTexture2D<float> stNEWGI_Z4 : register(u4);
cbuffer Host : register(b0) {
 uint Width; uint Height; float FarPlane; uint Reversed;
};
#define BUFFER_PIXEL_SIZE_DLSS (1.0/float2(Width,Height))
struct CSIN {
 uint3 groupthreadid : SV_GroupThreadID;
 uint3 groupid : SV_GroupID;
 uint3 dispatchthreadid : SV_DispatchThreadID;
 uint threadid : SV_GroupIndex;
};
float4 linearize_depth(float4 d) {
 d=Reversed ? 1-d : d;
 return saturate(d / (FarPlane-d*(FarPlane-1)));
}
float depth_to_z(float d) { return d*FarPlane+1; }
// All threads participate in group barriers, including the padded edge tile.
// Guard stores individually rather than returning early from the dispatch.
#define tex2Dstore(target,p,value) { uint tw,th; target.GetDimensions(tw,th); if(all(uint2(p)<uint2(tw,th))) target[uint2(p)]=value; }
float3 cone_overlap(float3 c)
{
    float k = 0.99 * 0.33;
    float2 f = float2(1 - 2 * k, k);
    float3x3 m = float3x3(f.xyy, f.yxy, f.yyx);
    return mul(c, m);
}
float3 unpack_hdr(float3 color)
{
    color = saturate(color);
    color = cone_overlap(color);
    color = color*0.283799*((2.52405+color)*color);    
    //color = srgb_to_AgX(color);
    color = color * rcp(1.04 - saturate(color));    
    return color;
}
uint2 morton_i_to_xy(uint i, uint N = 0) 
{    
    uint2 p = uint2(i, i >> 1);
    p &= 0x55555555;   
    p = (p ^ (p >> 1)) & 0x33333333; 
    p = (p ^ (p >> 2)) & 0x0F0F0F0F; 
    p = (p ^ (p >> 4)) & 0x00FF00FF; 
    p = (p ^ (p >> 8)) & 0x0000FFFF;
    return p;
}
struct ZDownsamplePayload
{    
    float depth;
    float energy;
    float2 minmax;
};

ZDownsamplePayload reduce_z(ZDownsamplePayload a, ZDownsamplePayload b, ZDownsamplePayload c, ZDownsamplePayload d)
{
    ZDownsamplePayload combined;
    combined.minmax = 0.25 * (a.minmax + b.minmax + c.minmax + d.minmax);

    float2 minmax = pow(combined.minmax, float2(-0.125, 0.125));
    float4 depths = float4(a.depth, b.depth, c.depth, d.depth);
    float4 energy = float4(a.energy, b.energy, c.energy, d.energy);

    const float sharpness = 9.0;

    float4 weights_near = exp(-max(1, depths / minmax.x) * sharpness);
    float4 weights_faar = exp(-max(1, minmax.y / depths) * sharpness);
    weights_near *= 0.1 + energy;
    weights_faar *= 0.1 + energy;

    float wsum_near = dot(weights_near, 1);
    float wsum_faar = dot(weights_faar, 1);

    float anchor = wsum_near > wsum_faar ? minmax.x : minmax.y;
    float4 weights = wsum_near > wsum_faar ? weights_near : weights_faar; 
    float wsum = max(wsum_near, wsum_faar);

    combined.depth = dot(depths, weights) / wsum;
    combined.energy = dot(energy, weights) / wsum;

    return combined;
}

ZDownsamplePayload _ZDownsamplePayload(float depth, float3 radiance)
{
    ZDownsamplePayload ts;
    ts.depth = depth;
    ts.energy = length(unpack_hdr(radiance));   
    ts.minmax = float2(pow(ts.depth, -8.0), pow(ts.depth, 8.0));
    return ts;
}

groupshared ZDownsamplePayload tile_z[16*16];

[numthreads(16,16,1)] void DownsampleDepthCS(CSIN i)
{
    uint2 p = (i.groupid.xy << 4) | morton_i_to_xy(i.threadid);
    uint4 tp; tp.xy = p * 2; tp.zw = tp.xy + 1;
   
    float2 quad_uv = tp.zw * BUFFER_PIXEL_SIZE_DLSS; 
    float4 quad_depths = DepthTex.GatherRed(ClampLinear, quad_uv); 
    quad_depths = linearize_depth(quad_depths);

#if RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN  // Flip vertically for gather order
    quad_depths = quad_depths.wzyx;     // WZ
#endif                                  // XY  
    
    float3 radianceX = ColorTex.SampleLevel(ClampLinear, quad_uv + BUFFER_PIXEL_SIZE_DLSS * float2(-0.5,  0.5), 0).rgb;  
    float3 radianceY = ColorTex.SampleLevel(ClampLinear, quad_uv + BUFFER_PIXEL_SIZE_DLSS * float2( 0.5,  0.5), 0).rgb; 
    float3 radianceZ = ColorTex.SampleLevel(ClampLinear, quad_uv + BUFFER_PIXEL_SIZE_DLSS * float2( 0.5, -0.5), 0).rgb;
    float3 radianceW = ColorTex.SampleLevel(ClampLinear, quad_uv + BUFFER_PIXEL_SIZE_DLSS * float2(-0.5, -0.5), 0).rgb;
   
    ZDownsamplePayload X = _ZDownsamplePayload(quad_depths.x, radianceX);  
    ZDownsamplePayload Y = _ZDownsamplePayload(quad_depths.y, radianceY);  
    ZDownsamplePayload Z = _ZDownsamplePayload(quad_depths.z, radianceZ);    
    ZDownsamplePayload W = _ZDownsamplePayload(quad_depths.w, radianceW); 

    tex2Dstore(stNEWGI_Z0, tp.xw, depth_to_z(X.depth));     
    tex2Dstore(stNEWGI_Z0, tp.zw, depth_to_z(Y.depth)); 
    tex2Dstore(stNEWGI_Z0, tp.zy, depth_to_z(Z.depth));  
    tex2Dstore(stNEWGI_Z0, tp.xy, depth_to_z(W.depth)); 

    ZDownsamplePayload combined = reduce_z(X, Y, Z, W); 
    tex2Dstore(stNEWGI_Z1, p, depth_to_z(combined.depth));

    tile_z[i.threadid] = combined;
    GroupMemoryBarrierWithGroupSync();
    if(!(i.threadid & 3))
    {
        tile_z[i.threadid] = combined = reduce_z(tile_z[i.threadid], tile_z[i.threadid + 1], tile_z[i.threadid + 2], tile_z[i.threadid + 3]);
        tex2Dstore(stNEWGI_Z2, p >> 1, depth_to_z(combined.depth)); 
    }
    GroupMemoryBarrierWithGroupSync();
    if(!(i.threadid & 15))
    {
        tile_z[i.threadid] = combined = reduce_z(tile_z[i.threadid], tile_z[i.threadid + 4], tile_z[i.threadid + 8], tile_z[i.threadid + 12]);   
        tex2Dstore(stNEWGI_Z3, p >> 2, depth_to_z(combined.depth)); 
    }
    GroupMemoryBarrierWithGroupSync();
    if(!(i.threadid & 63))
    {
        combined = reduce_z(tile_z[i.threadid], tile_z[i.threadid + 16], tile_z[i.threadid + 32], tile_z[i.threadid + 48]);
        tex2Dstore(stNEWGI_Z4, p >> 3, depth_to_z(combined.depth));        
    }
}

