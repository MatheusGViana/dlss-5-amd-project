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



#define PI 3.14159265358979323846
Texture2D<float> ZTex:register(t0);
Texture2D<float4> NormalTex:register(t1);
Texture2D<float4> GeoNormalTex:register(t2);
Texture2D<float4> NoiseTex:register(t3);
Texture2D<float4> PermutationTex:register(t4);
Texture2D<float> HorizonTex:register(t5);
Texture3D<float4> RadianceTex:register(t6);
RWTexture2D<float4> stNEWGI_Aux0:register(u0);
RWTexture2D<float> stNEWGI_STSGCache:register(u1);
SamplerState ClampLinear:register(s0);
SamplerState ClampPoint:register(s1);
cbuffer Host:register(b0) {
 uint Width;uint Height;float FarPlane;float TanHalfFov;
 uint FrameIndex;uint Quality;float Thickness;float FadeDepth;
 float AOAmount;float AmbientLevel;uint DebugView;uint Padding;
};
#define BUFFER_WIDTH_DLSS Width
#define BUFFER_SCREEN_SIZE_DLSS uint2(Width,Height)
#define BUFFER_PIXEL_SIZE_DLSS (1.0/float2(Width,Height))
#define BUFFER_ASPECT_RATIO_DLSS float2(1.0,float(Width)/Height)
#define FRAMECOUNT FrameIndex
#define DIFFUSE_GI_Q min(Quality,4u)
#define RT_Z_THICKNESS Thickness
#define RT_AO_AMOUNT AOAmount
#define RT_AMBIENT_LEVEL AmbientLevel
#define RT_FADE_DEPTH FadeDepth
#define RT_DEBUG_VIEW DebugView

#define atomicAdd(destination,value) InterlockedAdd(destination,value)
#define tex2Dstore(target,p,value) {uint tw,th;target.GetDimensions(tw,th);if(all(uint2(p)<uint2(tw,th)))target[uint2(p)]=value;}
struct CSIN{uint3 groupthreadid:SV_GroupThreadID;uint3 groupid:SV_GroupID;uint3 dispatchthreadid:SV_DispatchThreadID;uint threadid:SV_GroupIndex;};
float linearstep(float a,float b,float x){return saturate((x-a)/(b-a));}
float2 linearstep(float2 a,float2 b,float2 x){return saturate((x-a)/(b-a));}
namespace Camera {
 float depth_to_z(float d){return d*FarPlane+1;}
 float3 uv_to_proj(float2 uv,float z){return float3((uv*2-1)*float2(float(Width)/Height,1)*TanHalfFov*z,z);}
}
namespace Depth {float get_linear_depth(float2 uv){return saturate((ZTex.SampleLevel(ClampPoint,uv,0)-1)/FarPlane);}}
namespace Deferred {
 float3 get_normals(float2 uv){return NormalTex.SampleLevel(ClampPoint,uv,0).xyz;}
 float3 get_geometry_normals(float2 uv){return GeoNormalTex.SampleLevel(ClampPoint,uv,0).xyz;}
}
namespace Math {
 bool inside_screen(float2 uv){return all(uv>=0)&&all(uv<=1);}
 float fast_acos(float x){return acos(clamp(x,-1,1));}
 float2 aabb_hit_01(float2 origin,float2 dir){float2 hit=abs((dir<0?origin:1-origin)/dir);return origin+dir*min(hit.x,hit.y);}
}
namespace SphericalHarmonics 
{

float4 dir_to_sh(float3 v) 
{
    const float c0 = 0.5 * sqrt(1.0  / PI);
    const float c1 =       sqrt(0.75 / PI);
    return float4(c0, -c1 * v.y, c1 * v.z, -c1 * v.x);
}

//defaults to cosine convolution
float4 dir_to_irradiance_probe(float4 sh, float sharpness = 1)
{
	const float c0 = 2 - sharpness;
	const float c1 = sharpness * 0.66666;
    return sh * float4(c0, c1.xxx);
}

float3 linear_eval_irradiance(float4 sh_r, float4 sh_g, float4 sh_b, float3 v, float sharpness = 1)
{
    float4 sh_dir = dir_to_sh(v);
    sh_dir = dir_to_irradiance_probe(sh_dir, sharpness);//cosine conv
    return float3(dot(sh_r, sh_dir), dot(sh_g, sh_dir), dot(sh_b, sh_dir));
}

float3 hallucinate_zh3_irradiance(float4 sh_r, float4 sh_g, float4 sh_b, float3 v, float sharpness = 1)
{
    const float3 lum_coeffs = float3(0.2126, 0.7152, 0.0722);
    float3 zonal_axis = (sh_r.wyz * lum_coeffs.x + sh_g.wyz * lum_coeffs.y + sh_b.wyz * lum_coeffs.z) * float3(-1,-1,1);
    //zonal_axis = normalize(zonal_axis);//Deferring the multiply of that since we can just scale the scalar results of ops with this axis
    float invzonalaxislen = rsqrt(dot(zonal_axis, zonal_axis) + 1e-8);
    
    float3 ratio = abs(-float3(sh_r.w, sh_g.w, sh_b.w) * zonal_axis.x 
                     + -float3(sh_r.y, sh_g.y, sh_b.y) * zonal_axis.y 
                     +  float3(sh_r.z, sh_g.z, sh_b.z) * zonal_axis.z);
    ratio /= float3(sh_r.x, sh_g.x, sh_b.x);
    ratio *= invzonalaxislen;

    float3 zonal_l2_coeff = float3(sh_r.x, sh_g.x, sh_b.x) * ((0.6 * ratio + 0.08) * ratio);
    float fZ = dot(zonal_axis, v) * invzonalaxislen;
    float zh_dir = sqrt(5 / (16 * PI)) * (3 * fZ * fZ - 1);
    //cosine conv - technically incorrect to add a sharpness param here but I might need the flexibility.
    return linear_eval_irradiance(sh_r, sh_g, sh_b, v, sharpness) + 0.25 * zonal_l2_coeff * zh_dir;
}

}
namespace Hash {
uint uhash(uint x)
{
    x ^= x >> 16;
    x *= 0x21f0aaad;
    x ^= x >> 15;
    x *= 0xd35a2d97;
    x ^= x >> 16;
    return x;
}
float  uint_to_unorm  (uint  u){return asfloat((u >> 9u) | 0x3F800000u) - 1.0;}
}
namespace QMC {
float roberts1(in uint idx, in float seed = 0.5)
{
    uint useed = uint(seed * exp2(32.0));
    uint phi = 2654435769u;
    return float(phi * idx + useed) * exp2(-32.0);
}
}
float2 pixel_idx_to_uv(uint2 pos, float2 texture_size)
{
    float2 inv_texture_size = rcp(texture_size);
    return pos * inv_texture_size + 0.5 * inv_texture_size;
}
bool check_boundaries(uint2 pos, uint2 dest_size)
{
    return all(pos < dest_size) && all(pos >= uint2(0, 0));
}
float3 linear_to_ycocg(float3 c)
{
    float3 ycocg;
    ycocg.y = c.r - c.b;
    float tmp = c.b + ycocg.y * 0.5;
    ycocg.z = c.g - tmp;
    ycocg.x = tmp + ycocg.z * 0.5;
    return ycocg;
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
struct TraceContext
{
    float2 uv;
    uint2 texel; //xy: working pos, zw: write pos
    float3 pos; //view space position
    float3 normal;
    float3 viewdir;
    float depth;
    float4 jitter;
    float3 geonormal;
};

TraceContext _TraceContext(in CSIN i, in uint2 working_size)
{
    TraceContext o;
#if 0
    o.texel = i.dispatchthreadid.xy;
    uint2 jitter_texel = (i.dispatchthreadid.xy & 63u) + uint2(FRAMECOUNT & 15u, (FRAMECOUNT >> 4) & 7u) * 64u;
    o.jitter.xyz = NoiseTex.Load(int3(jitter_texel,0)).xyz;  
    jitter_texel = (i.dispatchthreadid.yx & 63u) + uint2(FRAMECOUNT & 15u, (FRAMECOUNT >> 4) & 7u) * 64u;
    o.jitter.w = NoiseTex.Load(int3(jitter_texel,0)).x;  
#else 
    uint2 jitter_texel = (i.dispatchthreadid.xy & 63u) + uint2(FRAMECOUNT & 15u, (FRAMECOUNT >> 4) & 7u) * 64u;
    float3 seed_data = PermutationTex.Load(int3(jitter_texel,0)).xyz;

    o.jitter.x = seed_data.x; 
    o.texel = i.groupid.xy * 32 + int2(seed_data.yz * 255.0 + 0.5); 
    
    jitter_texel = (o.texel & 63u) + uint2(FRAMECOUNT & 15u, (FRAMECOUNT >> 4) & 7u) * 64u;
    o.jitter.yzw = NoiseTex.Load(int3(jitter_texel,0)).xyz; 
#endif

    o.uv        = pixel_idx_to_uv(o.texel, working_size); 
    o.depth     = Depth::get_linear_depth(o.uv);
    o.pos       = Camera::uv_to_proj(o.uv, Camera::depth_to_z(o.depth));
    o.normal    = Deferred::get_normals(o.uv);
    o.viewdir   = normalize(o.pos);
    o.geonormal = Deferred::get_geometry_normals(o.uv);
    o.pos      *= 0.998; 
    return o;
}

float diffuse_sample_curve(float x)
{   
    float k = 5.0;
    return (exp2(k * x) - 1) / (exp2(k) - 1); 
}

float diffuse_sample_curve_inverse(float x)
{
    float k = 5.0;
    return log2(x * exp2(k) - x + 1) / k;
}


//quantized
#define FLOAT_TO_UINT_QUANTIZATION_SCALE     65536.0
#define NUM_DIRECTIONS                       64 //DO NOT CHANGE
#define TILE_DIMENSIONS                      8  //DO NOT CHANGE

groupshared float tgsm_pdf[1024];
groupshared float tgsm_cdf[1024];
groupshared uint  tgsm_pdf_accum[1024];

uint2 bitfieldocclusion64(float2 h_frontback, inout uint2 global_occlusion)
{
    float2 minh = linearstep(float2(0, 0.5), float2(0.5, 1), h_frontback.x);
    float2 maxh = linearstep(float2(0, 0.5), float2(0.5, 1), h_frontback.y);

    uint2 a = uint2(minh * 32);
    //uint3 b = ceil(saturate(maxh - minh) * 32);
    uint2 b = uint2(maxh * 32) - a;

    uint2 occlusion = ((1u << b) - 1u) << a;
    occlusion.x = b.x == 32 ? 0xFFFFFFFFu : occlusion.x; //full occlusion
    occlusion.y = b.y == 32 ? 0xFFFFFFFFu : occlusion.y; //full occlusion

    uint2 local_bitfield = global_occlusion & ~occlusion;
    uint2 changed_bits = local_bitfield ^ global_occlusion;
    global_occlusion = local_bitfield;
    return changed_bits;
}

float4 trace_diffuse_cdf_cubic(TraceContext ctx)
{ 
    if(!Math::inside_screen(ctx.uv) || can_earlyout(ctx.depth)) return 0;

    ctx.jitter.x = frac(ctx.jitter.x + Hash::uint_to_unorm(Hash::uhash(ctx.texel.x + ctx.texel.y * 195345)) / 255.0);  
    ctx.jitter.y = frac(ctx.jitter.y + Hash::uint_to_unorm(Hash::uhash(ctx.texel.y + ctx.texel.x * 195345)) / 255.0);  

    static const int quality_preset_steps[5] = {4, 12, 18, 32, 40};//you think you want to tamper with this, but you don't  
    static const int quality_preset_rays[5] = {1,  1,  1,  1,  1};//you think you want to tamper with this, but you don't  
   
    uint num_slices  = quality_preset_rays[DIFFUSE_GI_Q];
    uint sample_count = quality_preset_steps[DIFFUSE_GI_Q];

    float slicesum = 1e-6;
    float T = RT_Z_THICKNESS * RT_Z_THICKNESS;  //arbitrary thickness that looks good relative to sample radius

    float mip_bias = log2(BUFFER_WIDTH_DLSS) - 5.0 + (ctx.jitter.z - 0.5);

    float3 v = -ctx.viewdir;
    float3 n = ctx.normal; 

    int2 block_id = (ctx.texel % 32u) / TILE_DIMENSIONS;
    int num_blocks = 32 / TILE_DIMENSIONS;
    int flat_block_id = block_id.x + block_id.y * num_blocks;
    int block_start = flat_block_id * NUM_DIRECTIONS;

    float4 result = 0; 

    [loop]
    for(int slice_id = 0; slice_id < num_slices; slice_id++)
    {        
        float fi = float(slice_id + ctx.jitter.x) / num_slices;  
       
        int idx = block_start;
        idx = fi >= tgsm_cdf[idx + 32] ? idx + 32 : idx;
        idx = fi >= tgsm_cdf[idx + 16] ? idx + 16 : idx;
        idx = fi >= tgsm_cdf[idx +  8] ? idx +  8 : idx;      
        idx = fi >= tgsm_cdf[idx +  4] ? idx +  4 : idx;
        idx = fi >= tgsm_cdf[idx +  2] ? idx +  2 : idx;
        idx = fi >= tgsm_cdf[idx +  1] ? idx +  1 : idx;

        int local_idx = idx - block_start; 
        float cdf_this_bin = tgsm_cdf[idx];
        float cdf_next_bin = local_idx == (NUM_DIRECTIONS - 1) ? 1.0 : tgsm_cdf[idx + 1];
        float relative_pos_in_bracket = linearstep(cdf_this_bin, cdf_next_bin, fi);

        int idx_next_bin = (local_idx == (NUM_DIRECTIONS - 1)) ? block_start : idx + 1; //avoids modulo

        float pdf0 = tgsm_pdf[idx];
        float pdf1 = tgsm_pdf[idx_next_bin]; 
  
        //normalize removed here, check if bugs appear, see BAK Test 67
        float x = relative_pos_in_bracket;
        float icdf_this_bracket = (sqrt(lerp(pdf0*pdf0, pdf1*pdf1, x)) - pdf0) / (pdf1 - pdf0);//(-P + sqrt(max(0, P*P + (Q*Q - P*P) * x))) / (Q - P);

        float remapped_pos_in_bracket = abs(pdf1 - pdf0) < 1e-3 ? x : icdf_this_bracket; //avoid numerical instability  
        float pdf = lerp(pdf0, pdf1, remapped_pos_in_bracket);     

        float weight_this_bin = 1.0 - remapped_pos_in_bracket;
        float weight_next_bin = remapped_pos_in_bracket;           
        fi = (local_idx + remapped_pos_in_bracket) / NUM_DIRECTIONS;

        //actual body starts here
        float2 slice_dir; 
        sincos(fi * PI, slice_dir.y, slice_dir.x);        
      
        float3 ortho_dir = float3(slice_dir, 0) - dot(slice_dir, v.xy) * v; //z = 0 so no need for full dot3        
        float3 slice_n = cross(ortho_dir, v);
        float rcp_slice_n_len = rsqrt(dot(slice_n, slice_n)); //do not normalice slice_n, we can scale scalars with the inv len later

        float sin_n = dot(slice_n, n) * rcp_slice_n_len; //cos between slice normal and normal == sin between normal projected on slice vs normal itself
        float3 n_proj_on_slice = n - slice_n * (sin_n * rcp_slice_n_len);
        float proj_n_len = sqrt(saturate(1 - sin_n * sin_n));
        float cosn = saturate(dot(n_proj_on_slice, v) * rcp(proj_n_len+1e-6));
       
        float normal_angle = Math::fast_acos(cosn);
        normal_angle = dot(ortho_dir, n_proj_on_slice) > 0 ? normal_angle : -normal_angle;
        float sliceweight = max(0, (cosn + normal_angle * sin(normal_angle)) * proj_n_len);

        uint2 occlusion_bitfield = 0xFFFFFFFF;
   
        float2 scaled_dir = slice_dir * BUFFER_ASPECT_RATIO_DLSS; //verified 110125
        float2 initial_step = slice_dir * BUFFER_PIXEL_SIZE_DLSS;
        float4 slice_result = 0.0;

        [unroll]
        for(int side = 0; side < 2; side++)
        {    
            float2 limit_uv = Math::aabb_hit_01(ctx.uv, scaled_dir);
            float2 uv_delta = abs(limit_uv - ctx.uv);

            float dist_to_edge = length(uv_delta / BUFFER_ASPECT_RATIO_DLSS);
            int num_samples_this_dir = 1 + int(diffuse_sample_curve_inverse(dist_to_edge) * sample_count);           

            [loop]         
            for(int _sample = 0; _sample <= num_samples_this_dir; _sample++)
            { 
                float2 s = saturate((_sample + float2(0, 0.5) + ctx.jitter.y * 0.5) / sample_count); //yes actually sample count
                s.x = diffuse_sample_curve(s.x);
                s.y = diffuse_sample_curve(s.y); 
                s = min(s, dist_to_edge);  

                float mip = log2(s.y) + mip_bias;                    

                float4 tap_uvs;
                tap_uvs.xy = ctx.uv + initial_step + scaled_dir * s.x;
                tap_uvs.zw = ctx.uv + initial_step + scaled_dir * s.y;

                float2 zvals;             
                zvals.x = ZTex.SampleLevel(ClampPoint, tap_uvs.xy, mip);
                zvals.y = ZTex.SampleLevel(ClampPoint, tap_uvs.zw, mip);              
  
                [unroll]
                for(int pair = 0; pair < 2; pair++)
                {
                    float2 tap_uv = tap_uvs.xy;
                    float zz = zvals.x;
           
                    tap_uvs.xy = tap_uvs.zw;
                    zvals.x = zvals.y;    
   
                    float3 Lp = Camera::uv_to_proj(tap_uv, zz);          
                    float3 L1 = Lp - ctx.pos;                  
                    float3 L2 = L1 + Lp * T;  

                    float iL1L1 = rsqrt(dot(L1, L1));
                    float iL2L2 = rsqrt(dot(L2, L2));
                    float2 h = float2(dot(L1, v) * iL1L1, dot(L2, v) * iL2L2); //divide by length rather than normalize vector first, faster on scalar hardware

                    h = side ? (h * 0.25 + 0.25) : (h.yx * -0.25 + 0.75);
                    h.x = HorizonTex.SampleLevel(ClampLinear, float2(h.x, normal_angle / PI + 0.5), 0);
                    h.y = HorizonTex.SampleLevel(ClampLinear, float2(h.y, normal_angle / PI + 0.5), 0);   
                        
                    h = saturate(h + QMC::roberts1(slice_id, ctx.jitter.w) / 64.0);
                    uint2 changed_bits = bitfieldocclusion64(h, occlusion_bitfield); 

                    [branch]
                    if(any(changed_bits) && dot(L1, ctx.geonormal) > 0) //we need the latter to avoid self-occlusion weirdness               
                    {                       
                        float3 uvz = (min(max(mip - 2, 0), 4) + float3(0, 5, 10) + 0.5) / 15.0;    
                        float4 shr = RadianceTex.SampleLevel(ClampPoint,float3(tap_uv, uvz.x),0);
                        float4 shg = RadianceTex.SampleLevel(ClampPoint,float3(tap_uv, uvz.y),0);
                        float4 shb = RadianceTex.SampleLevel(ClampPoint,float3(tap_uv, uvz.z),0);  
                        
                        float2 bits = countbits(changed_bits);
                        float hit = (bits.x + bits.y) / 64.0;

                        slice_result += float4(max(4 * SphericalHarmonics::linear_eval_irradiance(shr, shg, shb, -L1, 2), 0) * (iL1L1 * hit), hit); //matched against ZH3
                    }                                                                  
                }          
            }
     
            scaled_dir = -scaled_dir;
            initial_step = -initial_step;
        }

        slice_result *= sliceweight; //apply it here outside the inner loop
        slice_result.w = 1 - slice_result.w; //works better for the importance sampling
     
        float target_pdf = dot(slice_result.rgb, float3(0.2125, 0.7126, 0.0722));  

        //sacrifice a little IL importance sampling strength for AO if its intensity is high
        target_pdf += slice_result.w * 0.01 * saturate(RT_AO_AMOUNT * 0.1) * saturate(1 - RT_AMBIENT_LEVEL); 

        target_pdf = 1;   

        //target_pdf = TURN_ON_THE_MAGIC ? target_pdf : 1.0; 
        atomicAdd(tgsm_pdf_accum[idx],          uint(target_pdf / pdf * (1.0 - relative_pos_in_bracket) * FLOAT_TO_UINT_QUANTIZATION_SCALE));
        atomicAdd(tgsm_pdf_accum[idx_next_bin], uint(target_pdf / pdf *        relative_pos_in_bracket  * FLOAT_TO_UINT_QUANTIZATION_SCALE));      
        result += slice_result / (pdf * NUM_DIRECTIONS + 1e-4);                 
    }
   
    result /= num_slices;
    return result;
}

[numthreads(32,32,1)] void TraceWrapCubicCS(in CSIN i)
{    
    int num_blocks = 32 / TILE_DIMENSIONS;
 
    //these are for the builder threads only, for writing the CDF we use the ids based off the shuffled pixels
    int id_in_block = i.threadid % NUM_DIRECTIONS;
    int block_start = i.threadid - id_in_block;
    int block_end   = block_start + (NUM_DIRECTIONS - 1);
    int2 pdf_storage_pos = i.groupid.xy * 32 + int2(i.threadid % 32, i.threadid / 32);

    float prev_pdf = stNEWGI_STSGCache[pdf_storage_pos];
    tgsm_pdf_accum[i.threadid] = 0;   
    tgsm_pdf[i.threadid] = prev_pdf + 0.005; //choke, do not change  
    GroupMemoryBarrierWithGroupSync();

    //init cubic prefix sum nodes, then perform sklansky style prefix sum
    if(id_in_block == 0)
        tgsm_cdf[i.threadid] = 0;
    else 
        tgsm_cdf[i.threadid] = (tgsm_pdf[i.threadid - 1] + tgsm_pdf[i.threadid]) * 0.5;
    
    GroupMemoryBarrierWithGroupSync();
        
    [unroll]
    for(uint b = 1, m = 0; b < NUM_DIRECTIONS; GroupMemoryBarrierWithGroupSync())
    {
        uint b2 = b * 2; uint m2 = b2 - 1; 
        if(i.threadid & b)       
            tgsm_cdf[i.threadid] += tgsm_cdf[(i.threadid & ~m2) + m];
        b = b2, m = m2;              
    }
    
    //normalize both PDF and CDF
    //the exclusive prefix sum for cubic interpolation is missing 0.5x the first and last entry
    float pdf_integral = tgsm_cdf[block_end]
                       + 0.5 * tgsm_pdf[block_start]
                       + 0.5 * tgsm_pdf[block_end];    
    GroupMemoryBarrierWithGroupSync(); 
    tgsm_pdf[i.threadid] /= pdf_integral;
    tgsm_cdf[i.threadid] /= pdf_integral;   
    GroupMemoryBarrierWithGroupSync();

    //actually perform the GI trace
    TraceContext ctx = _TraceContext(i, BUFFER_SCREEN_SIZE_DLSS);

    float4 gi = 0.0;
    if(check_boundaries(ctx.texel, BUFFER_SCREEN_SIZE_DLSS))     
        gi = trace_diffuse_cdf_cubic(ctx);     
    int2 write_id = ctx.texel & 31u;
    int write_thread = write_id.x + write_id.y * 32;

    tgsm_cdf[write_thread] = gi.x;GroupMemoryBarrierWithGroupSync();gi.x = tgsm_cdf[i.threadid];GroupMemoryBarrierWithGroupSync();   
    tgsm_cdf[write_thread] = gi.y;GroupMemoryBarrierWithGroupSync();gi.y = tgsm_cdf[i.threadid];GroupMemoryBarrierWithGroupSync(); 
    tgsm_cdf[write_thread] = gi.z;GroupMemoryBarrierWithGroupSync();gi.z = tgsm_cdf[i.threadid];GroupMemoryBarrierWithGroupSync(); 
    tgsm_cdf[write_thread] = gi.w;GroupMemoryBarrierWithGroupSync();gi.w = tgsm_cdf[i.threadid];GroupMemoryBarrierWithGroupSync(); 

    //filtering happens in YCoCg
    gi.rgb = linear_to_ycocg(gi.rgb);
    tex2Dstore(stNEWGI_Aux0, i.dispatchthreadid.xy, gi);
    GroupMemoryBarrierWithGroupSync();

    //read back the collected PDF values of current frame and normalize
    float curr_pdf = float(tgsm_pdf_accum[i.threadid]) / FLOAT_TO_UINT_QUANTIZATION_SCALE;
    tgsm_pdf[i.threadid] = curr_pdf;
    GroupMemoryBarrierWithGroupSync();
    
    [unroll]for(int stride = NUM_DIRECTIONS / 2; stride > 0; stride >>= 1)
    {
        if(id_in_block < stride)
            tgsm_pdf[i.threadid] += tgsm_pdf[i.threadid + stride];
        GroupMemoryBarrierWithGroupSync();
    }

    curr_pdf /= tgsm_pdf[block_start] + 1e-8; 

    //output interpolated PDF
    float integrated_pdf = lerp(prev_pdf, curr_pdf, 0.25);
    tex2Dstore(stNEWGI_STSGCache, pdf_storage_pos, integrated_pdf);    
}

/*=============================================================================
	Temporal Reprojection    
=============================================================================*/

