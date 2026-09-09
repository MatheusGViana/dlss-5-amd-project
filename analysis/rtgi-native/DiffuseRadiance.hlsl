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
Texture2D<float4> ColorTex : register(t0);
Texture2D<float> ZTex : register(t1);
Texture2D<float4> NormalTex : register(t2);
SamplerState ClampPoint : register(s1);
RWTexture3D<float4> stNEWGI_SHVolume : register(u0);
cbuffer Host : register(b0) { uint Width; uint Height; float FarPlane; float TanHalfFov; };
#define BUFFER_SCREEN_SIZE_DLSS uint2(Width,Height)
#define BUFFER_PIXEL_SIZE_DLSS (1.0/float2(Width,Height))
struct CSIN {uint3 groupthreadid:SV_GroupThreadID;uint3 groupid:SV_GroupID;uint3 dispatchthreadid:SV_DispatchThreadID;uint threadid:SV_GroupIndex;};
namespace Camera {
 float depth_to_z(float d){return d*FarPlane+1;}
 float3 uv_to_proj(float2 uv,float z){return float3((uv*2-1)*float2(float(Width)/Height,1)*TanHalfFov*z,z);}
}
namespace Deferred { float3 get_geometry_normals(float2 uv){return NormalTex.SampleLevel(ClampPoint,uv,0).xyz;} }
namespace Math {float inside_screen(float2 uv){return all(uv>=0)&&all(uv<=1);}}
#define tex3Dstore(target,p,v) {uint tw,th,td;target.GetDimensions(tw,th,td);if(all(uint3(p)<uint3(tw,th,td)))target[p]=v;}
float4 load_volume(int3 p){uint w,h,d;stNEWGI_SHVolume.GetDimensions(w,h,d);return all(p>=0)&&all(p<int3(w,h,d))?stNEWGI_SHVolume[p]:0;}
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
[numthreads(8,8,1)] void InitRadianceVolumeCS(in CSIN i)
{
    const uint2 target_size = BUFFER_SCREEN_SIZE_DLSS / 4;
    if(any(i.dispatchthreadid.xy >= target_size)) return;
    const float2 uv = (i.dispatchthreadid.xy + 0.5) / target_size;
    const float z0 = ZTex.SampleLevel(ClampPoint, uv, 2).x; //quarter res depth
    const float3 n0 = Deferred::get_geometry_normals(uv);
    const float3 p0 = Camera::uv_to_proj(uv, z0);

    float wsum = 0;
    float4 shr = 0;
    float4 shg = 0;
    float4 shb = 0;  

    float sky_thresh = Camera::depth_to_z(0.999);//easier to do it this way rather than converting Z back to depth every time

    [unroll]
    for(int x = 0; x < 4; ++x)
    {
        //4 way batching, faster
        const float2 tuv0 = (i.dispatchthreadid.xy * 4 + int2(x, 0) + 0.5) * BUFFER_PIXEL_SIZE_DLSS;  
        const float2 tuv1 = (i.dispatchthreadid.xy * 4 + int2(x, 1) + 0.5) * BUFFER_PIXEL_SIZE_DLSS; 
        const float2 tuv2 = (i.dispatchthreadid.xy * 4 + int2(x, 2) + 0.5) * BUFFER_PIXEL_SIZE_DLSS; 
        const float2 tuv3 = (i.dispatchthreadid.xy * 4 + int2(x, 3) + 0.5) * BUFFER_PIXEL_SIZE_DLSS; 

        float4 tz;
        tz.x = ZTex.SampleLevel(ClampPoint, tuv0, 0).x;
        tz.y = ZTex.SampleLevel(ClampPoint, tuv1, 0).x;
        tz.z = ZTex.SampleLevel(ClampPoint, tuv2, 0).x;
        tz.w = ZTex.SampleLevel(ClampPoint, tuv3, 0).x;     
        const float3 tp0 = Camera::uv_to_proj(tuv0, tz.x);
        const float3 tp1 = Camera::uv_to_proj(tuv1, tz.y);
        const float3 tp2 = Camera::uv_to_proj(tuv2, tz.z);
        const float3 tp3 = Camera::uv_to_proj(tuv3, tz.w);

        const float3 tn0 = Deferred::get_geometry_normals(tuv0);       
        const float3 tn1 = Deferred::get_geometry_normals(tuv1); 
        const float3 tn2 = Deferred::get_geometry_normals(tuv2); 
        const float3 tn3 = Deferred::get_geometry_normals(tuv3);  

        float4 plane_dist;
        plane_dist.x = max(abs(dot(tp0 - p0, n0)), abs(dot(tp0 - p0, tn0))) / z0 * 16.0;
        plane_dist.y = max(abs(dot(tp1 - p0, n0)), abs(dot(tp1 - p0, tn1))) / z0 * 16.0;
        plane_dist.z = max(abs(dot(tp2 - p0, n0)), abs(dot(tp2 - p0, tn2))) / z0 * 16.0;
        plane_dist.w = max(abs(dot(tp3 - p0, n0)), abs(dot(tp3 - p0, tn3))) / z0 * 16.0;
        float4 w = exp(-plane_dist * plane_dist);

        const float4 sh0 = SphericalHarmonics::dir_to_sh(tn0);
        const float4 sh1 = SphericalHarmonics::dir_to_sh(tn1);
        const float4 sh2 = SphericalHarmonics::dir_to_sh(tn2);
        const float4 sh3 = SphericalHarmonics::dir_to_sh(tn3);

        float3 radiance0 = ColorTex.SampleLevel(ClampPoint, tuv0, 0).rgb;
        float3 radiance1 = ColorTex.SampleLevel(ClampPoint, tuv1, 0).rgb;
        float3 radiance2 = ColorTex.SampleLevel(ClampPoint, tuv2, 0).rgb;
        float3 radiance3 = ColorTex.SampleLevel(ClampPoint, tuv3, 0).rgb; 

        radiance0 = unpack_hdr(radiance0) * step(tz.x, sky_thresh);
        radiance1 = unpack_hdr(radiance1) * step(tz.y, sky_thresh);
        radiance2 = unpack_hdr(radiance2) * step(tz.z, sky_thresh);
        radiance3 = unpack_hdr(radiance3) * step(tz.w, sky_thresh);
      
        shr += sh0 * (radiance0.r * w.x);
        shg += sh0 * (radiance0.g * w.x);
        shb += sh0 * (radiance0.b * w.x);

        shr += sh1 * (radiance1.r * w.y);
        shg += sh1 * (radiance1.g * w.y);
        shb += sh1 * (radiance1.b * w.y);

        shr += sh2 * (radiance2.r * w.z);
        shg += sh2 * (radiance2.g * w.z);
        shb += sh2 * (radiance2.b * w.z);

        shr += sh3 * (radiance3.r * w.w);
        shg += sh3 * (radiance3.g * w.w);
        shb += sh3 * (radiance3.b * w.w);

        wsum += dot(w, 1);
    }   

    wsum = rcp(wsum + 1e-6);
    shr *= wsum;
    shg *= wsum;
    shb *= wsum;

    int3 write_pos = int3(i.dispatchthreadid.xy, 0);
    tex3Dstore(stNEWGI_SHVolume, write_pos, shr); write_pos.z += 5;
    tex3Dstore(stNEWGI_SHVolume, write_pos, shg); write_pos.z += 5;
    tex3Dstore(stNEWGI_SHVolume, write_pos, shb);
}

//v1 entire plane in parallel
void downsample_volume_pass(in CSIN i, in uint it)
{
    int dilation = 1u << it;
    
    uint2 target_size = BUFFER_SCREEN_SIZE_DLSS / 4;
    if(any(i.dispatchthreadid.xy >= target_size)) return; 

    float2 uv = (i.dispatchthreadid.xy + 0.5) / target_size;
    float  z0 = ZTex.SampleLevel(ClampPoint, uv, 2).x; //quarter res depth TODO try out lower mips   
    float3 n0 = Deferred::get_geometry_normals(uv);
    float3 p0 = Camera::uv_to_proj(uv, z0);

    float4 shr = 0;
    float4 shg = 0;
    float4 shb = 0;
    float wsum = 0;

    [unroll]for(int x = -1; x <= 1; x++)
    [unroll]for(int y = -1; y <= 1; y++)    
    {
        int2 texel = i.dispatchthreadid.xy + int2(x, y) * dilation;
        float2 tuv = (texel + 0.5) / target_size;
        float  tz = ZTex.SampleLevel(ClampPoint, tuv, 2).x;
        float3 tn = Deferred::get_geometry_normals(tuv);
        float3 tp = Camera::uv_to_proj(tuv, tz);

        float plane_dist = max(abs(dot(tp - p0, n0)), abs(dot(tp - p0, tn))) / z0 * 16.0;
        float w = exp(-plane_dist * plane_dist);

        w *= Math::inside_screen(tuv); //alternatively use cropminmax
        shr += load_volume(int3(texel, it)) * w;
        shg += load_volume(int3(texel, it + 5)) * w;
        shb += load_volume(int3(texel, it + 10)) * w;
        wsum += w;
    }

    wsum = rcp(wsum + 1e-6);
    shr *= wsum;
    shg *= wsum;
    shb *= wsum;

    tex3Dstore(stNEWGI_SHVolume, int3(i.dispatchthreadid.xy, it + 1),      shr);
    tex3Dstore(stNEWGI_SHVolume, int3(i.dispatchthreadid.xy, it + 1 + 5),  shg);
    tex3Dstore(stNEWGI_SHVolume, int3(i.dispatchthreadid.xy, it + 1 + 10), shb);
}

[numthreads(16,16,1)] void PropagateRadianceCS0(in CSIN i){downsample_volume_pass(i, 0);}
[numthreads(16,16,1)] void PropagateRadianceCS1(in CSIN i){downsample_volume_pass(i, 1);}
[numthreads(16,16,1)] void PropagateRadianceCS2(in CSIN i){downsample_volume_pass(i, 2);}
[numthreads(16,16,1)] void PropagateRadianceCS3(in CSIN i){downsample_volume_pass(i, 3);}

/*=============================================================================
	Tracing
=============================================================================*/

