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



Texture2D<float> ZTex:register(t0);
Texture2D<float4> NormalTex:register(t1);
Texture2D<float4> GeoNormalTex:register(t2);
Texture2D<float2> MotionTex:register(t3);
Texture2D<float4> CurrentTex:register(t4);
Texture2D<float4> HistoryTex:register(t5);
Texture2D<float4> PreviousDataTex:register(t6);
Texture2D<float4> MomentsTex:register(t7);
Texture2D<float4> SignalTex:register(t8);
Texture2D<float4> GBufferTex:register(t9);
RWTexture2D<float4> stNEWGI_SpatialMoments:register(u0);
RWTexture2D<float4> stNEWGI_AccumDiff:register(u1);
RWTexture2D<float4> stNEWGI_PrevTemporalData:register(u2);
RWTexture2D<float4> SignalOut:register(u3);
RWTexture2D<float4> GBufferOut:register(u4);
SamplerState ClampLinear:register(s0);
SamplerState ClampPoint:register(s1);
cbuffer Host:register(b0){
 uint Width;uint Height;float FarPlane;float TanHalfFov;
 uint HistoryValid;uint FilterQuality;uint Iteration;float Smoothness;
 float FadeDepth;float AOAmount;float AmbientLevel;float ILAmount;
 uint DebugView;uint Pad0;uint Pad1;uint Pad2;
};
#define BUFFER_SCREEN_SIZE_DLSS uint2(Width,Height)
#define BUFFER_PIXEL_SIZE_DLSS (1.0/float2(Width,Height))
#define DENOISER_Q min(FilterQuality,2u)
#define FILTER_SMOOTHNESS Smoothness
#define RT_FADE_DEPTH FadeDepth
#define RT_AO_AMOUNT AOAmount
#define RT_AMBIENT_LEVEL AmbientLevel
#define RT_IL_AMOUNT ILAmount
#define RT_DEBUG_VIEW DebugView
#define VARIANCE_FP16_QUANTIZATION_SCALE 128.0
#define tex2Dstore(target,p,value) {uint tw,th;target.GetDimensions(tw,th);if(all(uint2(p)<uint2(tw,th)))target[uint2(p)]=value;}
struct CSIN{uint3 groupthreadid:SV_GroupThreadID;uint3 groupid:SV_GroupID;uint3 dispatchthreadid:SV_DispatchThreadID;uint threadid:SV_GroupIndex;};
struct VSOUT{float4 vpos;float2 uv;};
struct PSOUT2{float4 t0;float4 t1;};
namespace Camera {
 float z_to_depth(float z){return (z-1)/FarPlane;}
 float3 uv_to_proj(float2 uv,float z){return float3((uv*2-1)*float2(float(Width)/Height,1)*TanHalfFov*z,z);}
 float3 uv_to_proj(float2 uv){return uv_to_proj(uv,ZTex.SampleLevel(ClampPoint,uv,0));}
}
namespace Deferred {
 float3 get_normals(float2 uv){return NormalTex.SampleLevel(ClampPoint,uv,0).xyz;}
 float3 get_geometry_normals(float2 uv){return GeoNormalTex.SampleLevel(ClampPoint,uv,0).xyz;}
 float2 get_motion(float2 uv){return MotionTex.SampleLevel(ClampLinear,uv,0);}
}
namespace Math { bool inside_screen(float2 uv){return all(uv>=0)&&all(uv<=1);} float2 fast_sign(float2 v){return v>=0?1:-1;}
float2 octahedral_enc(in float3 v) 
{
    float2 result = v.xy * rcp(dot(abs(v), 1)); 
    float2 sgn = fast_sign(v.xy);
    result = v.z < 0 ? sgn - abs(result.yx) * sgn : result;
    return result * 0.5 + 0.5;
}
float3 octahedral_dec(float2 o) 
{
    o = o * 2.0 - 1.0;
    float3 v = float3(o.xy, 1.0 - abs(o.x) - abs(o.y));
    //v.xy = v.z < 0 ? (1.0 - abs(v.yx)) * fast_sign(v.xy) : v.xy;
    float t = saturate(-v.z);
    v.xy += v.xy >= 0.0.xx ? -t.xx : t.xx;
    return normalize(v);
}
}
namespace SFC {
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
}
float2 pixel_idx_to_uv(uint2 pos, float2 texture_size)
{
    float2 inv_texture_size = rcp(texture_size);
    return pos * inv_texture_size + 0.5 * inv_texture_size;
}
float get_fade_factor(float depth)
{   
    if(RT_DEBUG_VIEW) return 1;

    float fade = saturate(1 - depth * depth); //fixed fade that smoothly goes to 0 at depth = 1, to multiply on top 
    float t = depth / (1e-6 + RT_FADE_DEPTH * RT_FADE_DEPTH);
    return saturate(exp2(-t) * fade - 0.01); //so it actually reaches 0    
}
bool can_earlyout(float depth)
{
    return get_fade_factor(depth) < 1e-5;
}
groupshared float4 moments_tgsm[16 * 16];

//2x2 downsample and gaussian blur
//+- 5 gaussian blur ~ 5x5 gaussian blur on halfres data
//8x8 group that does tiled can reach a 16x16 area
[numthreads(8,8,1)] void SpatialMomentsCS(CSIN i)
{
    [unroll]for(int y = 0; y < 2; ++y)
    [unroll]for(int x = 0; x < 2; ++x) 
    {
        //process 2x2 blocks, each 8x8 sized, centered on the current 8x8 group
        int2 p = i.groupid.xy * 8 + i.groupthreadid.xy + uint2(x, y) * 8 - 4;
        //we're in halfres, so let's go fullres
        //gather needs a +0.5 offset, and we also need an offset of +0.5 to to shift to quad centers
        float2 gather_uv = (p * 2 + 1) * BUFFER_PIXEL_SIZE_DLSS;

        float4 m1_curr = CurrentTex.GatherRed(ClampLinear,gather_uv);
        float4 m1_prev = HistoryTex.GatherRed(ClampLinear,gather_uv); 
        //average in quad 
        float4 m;
        m.x = dot(0.25, m1_curr);
        m.y = dot(0.25, m1_curr * m1_curr);  
        m.z = dot(0.25, m1_prev);
        m.w = dot(0.25, m1_prev * m1_prev);
        int2 tgsm_pos = i.groupthreadid.xy + int2(x, y) * 8;
        moments_tgsm[tgsm_pos.y * 16 + tgsm_pos.x] = m;
    }
    GroupMemoryBarrierWithGroupSync();

    //7x7 gaussian blur in smem
    float4 res = 0;
    float wsum = 0;
    [unroll]for(int x = -3; x <= 3; ++x)
    [unroll]for(int y = -3; y <= 3; ++y)
    {
        int2 tgsm_pos = i.groupthreadid.xy + 4 + int2(x, y);
        float w = exp(-(x*x+y*y) / 9.0 * 0.5 * 4);
        res += moments_tgsm[tgsm_pos.y * 16 + tgsm_pos.x] * w;
        wsum += w;
    }
    res /= wsum; 

    //we output mean | variance | mean | variance
    //             CURR       |       PREV
    res.yw = sqrt(max(0, res.yw - res.xz * res.xz));  
    tex2Dstore(stNEWGI_SpatialMoments, i.dispatchthreadid.xy, res);   
}

float lanczos2( float x )
{ 		
    float t = saturate(x * x * 0.25);//mul, mul_sat
    float res = 1 - 4.0/9.0 * t;//mad
    res = res - res * t;//mad
    res *= res;//mul
    res = res - res * t; //mad
    res *= 1 - 4 * t;//mad, mul
    return res;
}

float decode_temporal_variance(float v)
{    
    v *= v;  
    v /= VARIANCE_FP16_QUANTIZATION_SCALE;
    return v;
}

float encode_temporal_variance(float v)
{
    v *= VARIANCE_FP16_QUANTIZATION_SCALE;           
    v = sqrt(v);   
    return v;
}

void TemporalReprojectionPS(in VSOUT i, out PSOUT2 o)
{
    float2 prev_uv = i.uv + Deferred::get_motion(i.uv);
    //prev_uv = i.uv + float2(1, 1) * BUFFER_PIXEL_SIZE_DLSS;
    bool inside_screen = HistoryValid && Math::inside_screen(prev_uv); 
    bool valid_history = inside_screen;

    float3 n = Deferred::get_normals(i.uv);
    float3 p = Camera::uv_to_proj(i.uv);    
    float  z = p.z;
    
    float NdotV = dot(p, n) * rsqrt(dot(p, p));
    float4 prev_diff = HistoryValid ? HistoryTex.SampleLevel(ClampLinear,prev_uv,0) : 0; //YCoCg AO

    float prev_variance = 0;
    float history_confidence = 0;

    [branch]
    if(inside_screen)
    {       
        //16x fetch gbuffer -> 4x gather for depth, 2x gather for normals, 1x gather for variance 
        float2 texel_uv = prev_uv * BUFFER_SCREEN_SIZE_DLSS - 0.5;
        int2 texel_lower = floor(texel_uv);
        float2 bilinear_kernel = frac(texel_uv);
        float4 bilinX = float4(0, 1 - frac(texel_uv.x), frac(texel_uv.x), 0);
        float4 bilinY = float4(0, 1 - frac(texel_uv.y), frac(texel_uv.y), 0); 

        float4 bilinear_weights; 
        bilinear_weights.x = (1 - bilinear_kernel.x) * (1 - bilinear_kernel.y);
        bilinear_weights.y =      bilinear_kernel.x  * (1 - bilinear_kernel.y);
        bilinear_weights.z = (1 - bilinear_kernel.x) *      bilinear_kernel.y;
        bilinear_weights.w =      bilinear_kernel.x  *      bilinear_kernel.y;
        float4 quad_anchors = float4(texel_lower.xyxy + float4(0, 0, 2, 2)) * BUFFER_PIXEL_SIZE_DLSS.xyxy;

        //first, gather depth in 4 quadrants
        float4 quad_z_00 = PreviousDataTex.GatherRed(ClampLinear,quad_anchors.xy).wzxy;  //XY|XY
        float4 quad_z_10 = PreviousDataTex.GatherRed(ClampLinear,quad_anchors.zy).wzxy;  //ZW|ZW   inner:   XY
        float4 quad_z_01 = PreviousDataTex.GatherRed(ClampLinear,quad_anchors.xw).wzxy;  //XY|XY            ZW
        float4 quad_z_11 = PreviousDataTex.GatherRed(ClampLinear,quad_anchors.zw).wzxy;  //ZW|ZW

        //-128 * abs(quad_z_00 - z) / z * abs(NdotV)
        //-128 * abs(quad_z_00 / z - z / z) * abs(NdotV)
        //-128 * abs(quad_z_00 / z - 1) * abs(NdotV)
        //-128 * abs(quad_z_00 / z * abs(NdotV) - abs(NdotV))
        //-abs(quad_z_00 / z * abs(NdotV) * 128 - abs(NdotV) * 128)
        float2 scalemad = float2(rcp(z), -1) * abs(NdotV) * 128;
        float4 reject_00 = exp2(-abs(quad_z_00 * scalemad.x + scalemad.y));
        float4 reject_10 = exp2(-abs(quad_z_10 * scalemad.x + scalemad.y));
        float4 reject_01 = exp2(-abs(quad_z_01 * scalemad.x + scalemad.y));
        float4 reject_11 = exp2(-abs(quad_z_11 * scalemad.x + scalemad.y));

        float4 octnx = PreviousDataTex.GatherGreen(ClampLinear,lerp(quad_anchors.xy, quad_anchors.zw, 0.5)).wzxy;
        float4 octny = PreviousDataTex.GatherBlue(ClampLinear,lerp(quad_anchors.xy, quad_anchors.zw, 0.5)).wzxy;
        //first, use normals and depth weight in 2x2 center
        float4 centerweights_z = float4(reject_00.w, reject_10.z, reject_01.y, reject_11.x);
        float4 centerweights_n;
        centerweights_n.x = saturate(dot(Math::octahedral_dec(float2(octnx.x, octny.x)), n));
        centerweights_n.y = saturate(dot(Math::octahedral_dec(float2(octnx.y, octny.y)), n));
        centerweights_n.z = saturate(dot(Math::octahedral_dec(float2(octnx.z, octny.z)), n));
        centerweights_n.w = saturate(dot(Math::octahedral_dec(float2(octnx.w, octny.w)), n));
     
        float4 lanczosX, lanczosY;       
        lanczosX.x = lanczos2(-bilinear_kernel.x - 1);  
        lanczosX.y = lanczos2(-bilinear_kernel.x    ); 
        lanczosX.z = lanczos2(-bilinear_kernel.x + 1); 
        lanczosX.w = lanczos2(-bilinear_kernel.x + 2); 
        lanczosY.x = lanczos2(-bilinear_kernel.y - 1);  
        lanczosY.y = lanczos2(-bilinear_kernel.y    ); 
        lanczosY.z = lanczos2(-bilinear_kernel.y + 1); 
        lanczosY.w = lanczos2(-bilinear_kernel.y + 2);
        //accumulate bilinear (and lanczos) in center
        float4 gi_00_center = HistoryTex.Load(int3(texel_lower + int2(0, 0),0));
        float4 gi_10_center = HistoryTex.Load(int3(texel_lower + int2(1, 0),0));
        float4 gi_01_center = HistoryTex.Load(int3(texel_lower + int2(0, 1),0));
        float4 gi_11_center = HistoryTex.Load(int3(texel_lower + int2(1, 1),0)); 
        //bilinear, z and normal
        float4 bilinear_combined_weights = bilinear_weights * centerweights_z * centerweights_n;
        float4 mean_bilinear = 0;
        mean_bilinear += gi_00_center * bilinear_combined_weights.x;
        mean_bilinear += gi_10_center * bilinear_combined_weights.y;
        mean_bilinear += gi_01_center * bilinear_combined_weights.z;
        mean_bilinear += gi_11_center * bilinear_combined_weights.w;
        //lanczos, only z
        float4 mean_lanczos = 0; 
        float4 lanczos_combined_weights = lanczosX.yzyz * lanczosY.yyzz * centerweights_z;
        mean_lanczos += gi_00_center * lanczos_combined_weights.x;
        mean_lanczos += gi_10_center * lanczos_combined_weights.y;
        mean_lanczos += gi_01_center * lanczos_combined_weights.z;
        mean_lanczos += gi_11_center * lanczos_combined_weights.w;
                
        float4 variances = PreviousDataTex.GatherAlpha(ClampLinear,lerp(quad_anchors.xy, quad_anchors.zw, 0.5)).wzxy;
        variances.x = decode_temporal_variance(variances.x);
        variances.y = decode_temporal_variance(variances.y);
        variances.z = decode_temporal_variance(variances.z);
        variances.w = decode_temporal_variance(variances.w);
        prev_variance = dot(bilinear_combined_weights, variances); 

        //also track minmax
        float4 minv = min(min(gi_00_center, gi_10_center), min(gi_01_center, gi_11_center));
        float4 maxv = max(max(gi_00_center, gi_10_center), max(gi_01_center, gi_11_center));
        //then add the other lanczos taps 
        float4 tapA, tapB;
        //2 leftleft
        float2 weights_lanczos_ll = lanczosX.xx * lanczosY.yz * float2(reject_00.z, reject_01.x);
        tapA = HistoryTex.Load(int3(texel_lower + int2(-1, 0),0));
        tapB = HistoryTex.Load(int3(texel_lower + int2(-1, 1),0));
        mean_lanczos += tapA * weights_lanczos_ll.x;
        mean_lanczos += tapB * weights_lanczos_ll.y;
        maxv = max(maxv, max(tapA, tapB));
        minv = min(minv, min(tapA, tapB));
        //2 rightright
        float2 weights_lanczos_rr = lanczosX.ww * lanczosY.yz * float2(reject_10.w, reject_11.y);
        tapA = HistoryTex.Load(int3(texel_lower + int2(2, 0),0));
        tapB = HistoryTex.Load(int3(texel_lower + int2(2, 1),0));
        mean_lanczos += tapA * weights_lanczos_rr.x;
        mean_lanczos += tapB * weights_lanczos_rr.y;
        maxv = max(maxv, max(tapA, tapB));
        minv = min(minv, min(tapA, tapB));
        //2 toptop
        float2 weights_lanczos_tt = lanczosX.yz * lanczosY.xx * float2(reject_00.y, reject_10.x);
        tapA = HistoryTex.Load(int3(texel_lower + int2(0, -1),0));
        tapB = HistoryTex.Load(int3(texel_lower + int2(1, -1),0));
        mean_lanczos += tapA * weights_lanczos_tt.x;
        mean_lanczos += tapB * weights_lanczos_tt.y;
        maxv = max(maxv, max(tapA, tapB));
        minv = min(minv, min(tapA, tapB));
        //2 bottombottom
        float2 weights_lanczos_bb = lanczosX.yz * lanczosY.ww * float2(reject_01.w, reject_11.z);
        tapA = HistoryTex.Load(int3(texel_lower + int2(0, 2),0));
        tapB = HistoryTex.Load(int3(texel_lower + int2(1, 2),0));
        mean_lanczos += tapA * weights_lanczos_bb.x;
        mean_lanczos += tapB * weights_lanczos_bb.y;
        maxv = max(maxv, max(tapA, tapB));
        minv = min(minv, min(tapA, tapB));

        float wsum_bilinear = dot(bilinear_combined_weights, 1);
        float wsum_lanczos = dot(lanczos_combined_weights, 1) + dot(weights_lanczos_ll + weights_lanczos_rr + weights_lanczos_tt + weights_lanczos_bb, 1);

        prev_variance /= wsum_bilinear + 1e-6;
        mean_bilinear /= wsum_bilinear + 1e-6;
        mean_lanczos /= (abs(wsum_lanczos) + 1e-6) * (wsum_lanczos >= 0 ? 1 : -1);

        prev_diff = lerp(mean_bilinear, mean_lanczos, saturate(wsum_bilinear * 4));
        prev_diff = clamp(prev_diff, minv, maxv); 

        if(wsum_bilinear < 0.05)
        {
            valid_history = false;
        }  

        history_confidence = wsum_bilinear;  
    }   

    float2 curr_data = MomentsTex.SampleLevel(ClampLinear,i.uv,0).xy;
    float2 prev_data = MomentsTex.SampleLevel(ClampLinear,prev_uv,0).zw;

    float bias = abs(curr_data.x - prev_data.x);
    float var_x = prev_data.y * prev_data.y;
    float var_y = curr_data.y * curr_data.y;
    float denom = exp2(-32.0) + var_x + var_y + bias * bias;    
    float alpha = saturate(1.0 - var_y / denom);
    alpha = clamp(alpha, 0.01, 0.15);

    float4 curr_diff = CurrentTex.SampleLevel(ClampLinear,i.uv,0);

    float X = prev_diff.x;
    float Y = curr_diff.x; 
    float variance_update = (X - Y) * (lerp(X, Y, alpha) - Y);   

    if(!valid_history)
    {
        variance_update = var_y * 16.0; //substitute a sensible high value that'll cause lots of blur
        alpha = 1;
    } 

    variance_update *= alpha * 0.5; //make it equivalent to spatial variance    
    float temporal_variance = lerp(prev_variance, variance_update, alpha);

    bool early_out = can_earlyout(Camera::z_to_depth(p.z));

    o.t0 = lerp(prev_diff, curr_diff, alpha); 
    //z negative -> can early out, easier to store than to do it in every filter pass. abs() on inputs is free.   
    o.t1 = float4(early_out ? -p.z : p.z, Math::octahedral_enc(n), encode_temporal_variance(temporal_variance)); 
}

[numthreads(8,8,1)] void UpdateHistoryCS(in CSIN i)
{    
    const int groupsize = 8;
    int2 p = i.groupid.xy * groupsize * 2 + SFC::morton_i_to_xy(i.threadid).yx;
    int2 p00 = p;
    int2 p01 = int2(p.x + groupsize, p.y);
    int2 p10 = int2(p.x, p.y + groupsize);
    int2 p11 = int2(p.x + groupsize, p.y + groupsize);
    //no storages as function parameters means copypaste goes brr
    {
        float4 t00 = SignalTex.Load(int3(p00,0));
        float4 t01 = SignalTex.Load(int3(p01,0));
        float4 t10 = SignalTex.Load(int3(p10,0));
        float4 t11 = SignalTex.Load(int3(p11,0));

        tex2Dstore(stNEWGI_AccumDiff, p00, t00);
        tex2Dstore(stNEWGI_AccumDiff, p01, t01);
        tex2Dstore(stNEWGI_AccumDiff, p10, t10);
        tex2Dstore(stNEWGI_AccumDiff, p11, t11);
    }
    {
        float4 t00 = GBufferTex.Load(int3(p00,0));
        float4 t01 = GBufferTex.Load(int3(p01,0));
        float4 t10 = GBufferTex.Load(int3(p10,0));
        float4 t11 = GBufferTex.Load(int3(p11,0));

        tex2Dstore(stNEWGI_PrevTemporalData, p00, t00);
        tex2Dstore(stNEWGI_PrevTemporalData, p01, t01);
        tex2Dstore(stNEWGI_PrevTemporalData, p10, t10);
        tex2Dstore(stNEWGI_PrevTemporalData, p11, t11);
    }
}

/*=============================================================================
	Denoise
=============================================================================*/

float get_signal_weight(float m1, float m2, float v1, float v2, float sharpness)
{
    float a = rsqrt(v1 + v1);
    float bias = (m1 - m2) * a;
    return sqrt(v1) * a * exp(-(0.5 * sharpness) * bias * bias) * 1.414;   
}

void atrous_pass(in int2 center_texel, 
                 Texture2D<float4> s_diff,
                 Texture2D<float4> s_gbuf,
                 const int it,
                 out PSOUT2 filter_out)
{
    if(it > (DENOISER_Q * 2 + 1)){filter_out.t0=s_diff.Load(int3(center_texel,0));filter_out.t1=s_gbuf.Load(int3(center_texel,0));return;} //0->1    1->3   2->5

    int scale = (DENOISER_Q ? 1 : 2) << it;
    int2 offsets[8] = 
    {
        int2(-1, -1) * scale, int2(0, -1) * scale, int2(1, -1) * scale,
        int2(-1,  0) * scale,                      int2(1,  0) * scale,
        int2(-1,  1) * scale, int2(0,  1) * scale, int2(1,  1) * scale
    };

    float2 center_uv = pixel_idx_to_uv(center_texel, BUFFER_SCREEN_SIZE_DLSS);
    float4 center_gbuf = s_gbuf.Load(int3(center_texel,0));
    float4 center_diff = s_diff.Load(int3(center_texel,0));

    filter_out.t0 = center_diff;
    filter_out.t1 = center_gbuf;

    bool early_out = center_gbuf.x < 0;
    if(early_out)
    {
        filter_out.t0 = float4(0, 0, 0, 1); //AO inverted.
        return;
    }

    float3 center_pos       = Camera::uv_to_proj(center_uv, abs(center_gbuf.x));//undo sign flag
    float3 center_normal    = Math::octahedral_dec(center_gbuf.yz);
    float3 center_geonormal = Deferred::get_geometry_normals(center_uv);
    float  center_var       = decode_temporal_variance(center_gbuf.w); 

    float variances[8];
    float weights[8];

    [unroll]
    for(int j = 0; j < 8; j++)
    {        
        float4 gbuf = s_gbuf.Load(int3(center_texel + offsets[j],0));

        float  tap_z = abs(gbuf.x); //undo sign flag
        float3 tap_n = Math::octahedral_dec(gbuf.yz);
        float  tap_v = gbuf.w;        
  
        float2 uv = pixel_idx_to_uv(center_texel + offsets[j], BUFFER_SCREEN_SIZE_DLSS);
        float3 deltav = Camera::uv_to_proj(uv, tap_z) - center_pos;
        float plane_dist = abs(dot(deltav, center_geonormal));
        float eucli_dist = length(deltav);
        float dist = lerp(plane_dist, eucli_dist, 0.25) / center_pos.z;

        float wz = dist * 100.0;
        wz = exp2(-wz * wz); 
 
        float wn = saturate(exp((dot(tap_n, center_normal) - 1) * 64.0));
      
        weights[j]   = lerp(0.001, 1, saturate(wz * wn));
        variances[j] = decode_temporal_variance(tap_v);
    }   

    float sharpness = saturate(1 - sqrt(FILTER_SMOOTHNESS) * 0.98);
    sharpness *= sharpness;
    
    float wsum = 1;
    filter_out.t0 = 0;
    filter_out.t1 = float4(center_gbuf.xyz, center_var);

    float4 minv = 1e10;
    float4 maxv = -1e10;

    [loop]
    for(int j = 0; j < 8; j++)
    {
        int2 texel = center_texel + offsets[j];
        if(any(texel < 0) || any(texel >= BUFFER_SCREEN_SIZE_DLSS)) continue;

        float4 tap_diff = s_diff.Load(int3(texel,0));        
        float w = weights[j] * get_signal_weight(center_diff.x, 
                               tap_diff.x, 
                               exp2(-32.0) + center_var, 
                               exp2(-32.0) + variances[j], sharpness);       
        wsum += w;
        filter_out.t0   += tap_diff     * w;
        filter_out.t1.w += variances[j] * w * w;       

        minv = min(minv, tap_diff);
        maxv = max(maxv, tap_diff);
    }

    [flatten]
    if(it < 0.5)
    {
        center_diff = clamp(center_diff, minv, maxv);
    }

    filter_out.t0 += center_diff;   
    filter_out.t0   /= wsum;
    filter_out.t1.w /= wsum * wsum;  //schied et al 2017     
    filter_out.t1.w = encode_temporal_variance(filter_out.t1.w);

    //last iteration postamble. 
    //Could be done in the blend pass but it requires depth, and thus is ill-suited
    //for TAAU, hence we do it here.
    [branch]
    if(it == (DENOISER_Q * 2 + 1))
    {  
        //apply fade and overall scaling here. No ambient yet since that is constant for every pixel.
        float fade = 1 - get_fade_factor(Camera::z_to_depth(center_pos.z));
        float rtao = saturate(filter_out.t0.w);
        rtao = lerp(1, rtao, saturate(RT_AO_AMOUNT * 0.1)) * saturate(RT_AMBIENT_LEVEL);
        rtao = lerp(rtao, 1, fade);        

        float3 diff_gi = filter_out.t0.rgb;
        diff_gi *= RT_IL_AMOUNT * RT_IL_AMOUNT * 2;
        diff_gi = lerp(diff_gi, 0, fade);
        filter_out.t0 = float4(diff_gi, rtao);
    }
}


[numthreads(8,8,1)] void TemporalReprojectionCS(CSIN input){
 uint2 p=input.dispatchthreadid.xy;if(any(p>=uint2(Width,Height)))return;
 VSOUT i;i.vpos=float4(p,0,1);i.uv=(float2(p)+.5)/float2(Width,Height);PSOUT2 o;
 TemporalReprojectionPS(i,o);SignalOut[p]=o.t0;GBufferOut[p]=o.t1;
}
[numthreads(8,8,1)] void DenoiseCS(CSIN input){
 uint2 p=input.dispatchthreadid.xy;if(any(p>=uint2(Width,Height)))return;
 PSOUT2 o;atrous_pass(p,SignalTex,GBufferTex,Iteration,o);SignalOut[p]=o.t0;GBufferOut[p]=o.t1;
}
