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
Texture2D<float4> ColorTex:register(t1);
Texture2D<float4> DiffuseTex:register(t2);
Texture2D<float4> AlbedoTex:register(t3);
RWTexture2D<float4> NormalOut:register(u0);
RWTexture2D<float4> ColorOut:register(u1);
SamplerState ClampPoint:register(s1);
cbuffer Host:register(b0){uint Width;uint Height;float FarPlane;float TanHalfFov;float PreExposure;float Mix;uint Inspect;uint Padding;};
#define BUFFER_PIXEL_SIZE_DLSS (1.0/float2(Width,Height))
struct VSOUT{float4 vpos;float2 uv;};
namespace Camera{
 float depth_to_z(float d){return d*FarPlane+1;}
 float3 uv_to_proj(float2 uv,float z){return float3((uv*2-1)*float2(float(Width)/Height,1)*TanHalfFov*z,z);}
}
namespace Depth {float get_linear_depth(float2 uv){return saturate((ZTex.SampleLevel(ClampPoint,uv,0)-1)/FarPlane);}}
float3 Encode(float3 c){c=max(c,0)/PreExposure;return pow(c/(1+c),1.0/2.2);}
float3 Decode(float3 c){c=pow(clamp(c,0,.9999),2.2);return c/max(1-c,.00001)*PreExposure;}
float3 cone_overlap(float3 c)
{
    float k = 0.99 * 0.33;
    float2 f = float2(1 - 2 * k, k);
    float3x3 m = float3x3(f.xyy, f.yxy, f.yyx);
    return mul(c, m);
}
float3 cone_overlap_inv(float3 c)
{
    float k = 0.99 * 0.33;
    float2 f = float2(k - 1, k) * rcp(3 * k - 1);
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
float3 pack_hdr(float3 color)
{
    color = 1.04 * color * rcp(color + 1.0);   
    //color = AgX_to_srgb(color);    
    color  = saturate(color);
    color = 1.14374*(-0.126893*color+sqrt(color));
    color = cone_overlap_inv(color);
    return color;     
}
float3 ycocg_to_linear(float3 c)
{
    float3 rgb;
    float tmp = c.x - c.z * 0.5;    
    rgb.g = c.z + tmp;
    rgb.b = tmp - c.y * 0.5;
    rgb.r = c.y + rgb.b;
    return rgb;
}
void NormalsPS(in VSOUT i, out float4 o)
{
	const float2 dirs[9] = 
	{
		BUFFER_PIXEL_SIZE_DLSS * float2(-1,-1),//TL
		BUFFER_PIXEL_SIZE_DLSS * float2(0,-1),//T
		BUFFER_PIXEL_SIZE_DLSS * float2(1,-1),//TR
		BUFFER_PIXEL_SIZE_DLSS * float2(1,0),//R
		BUFFER_PIXEL_SIZE_DLSS * float2(1,1),//BR
		BUFFER_PIXEL_SIZE_DLSS * float2(0,1),//B
		BUFFER_PIXEL_SIZE_DLSS * float2(-1,1),//BL
		BUFFER_PIXEL_SIZE_DLSS * float2(-1,0),//L
		BUFFER_PIXEL_SIZE_DLSS * float2(-1,-1)//TL first duplicated at end cuz it might be best pair	
	};

	float z_center = Depth::get_linear_depth(i.uv);
	float3 center_pos = Camera::uv_to_proj(i.uv, Camera::depth_to_z(z_center));

	//z close/far
	float2 z_prev;
	z_prev.x = Depth::get_linear_depth(i.uv + dirs[0]);
	z_prev.y = Depth::get_linear_depth(i.uv + dirs[0] * 2);
	float3 dv_prev = Camera::uv_to_proj(i.uv + dirs[0], Camera::depth_to_z(z_prev.x)) - center_pos;

	float4 best_normal = float4(0,0,0,100000);
	float4 weighted_normal = 0;

	[unroll]
	for(int j = 1; j < 9; j++)
	{
		float2 z_curr;
		z_curr.x = Depth::get_linear_depth(i.uv + dirs[j]);
		z_curr.y = Depth::get_linear_depth(i.uv + dirs[j] * 2);

		float3 dv_curr = Camera::uv_to_proj(i.uv + dirs[j], Camera::depth_to_z(z_curr.x)) - center_pos;	
		float3 temp_normal = cross(dv_curr, dv_prev);

		float2 z_guessed = 2 * float2(z_prev.x, z_curr.x) - float2(z_prev.y, z_curr.y);
		float error = dot(1, abs(z_guessed - z_center));
		
		float w = rcp(max(dot(temp_normal, temp_normal),1e-20));
		w *= rcp(error * error + exp2(-32.0));
		
		weighted_normal += float4(temp_normal, 1) * w;	
		best_normal = error < best_normal.w ? float4(temp_normal, error) : best_normal;

		z_prev = z_curr;
		dv_prev = dv_curr;
	}

	float3 normal = weighted_normal.w < 1.0 ? best_normal.xyz : weighted_normal.xyz;
	//normal = best_normal.xyz;
	normal /= max(max(abs(normal.x),max(abs(normal.y),abs(normal.z))),1e-30); normal *= rsqrt(max(dot(normal,normal),1e-20));
	//V2 geom normals to .zw	
	o = float4(dot(normal,normal)>0.5 ? normal : float3(0,0,-1),1);//fixes bugs in RTGI, normal.z positive gives smaller error :)	
}
[numthreads(8,8,1)] void PrepareNormalsCS(uint3 tid:SV_DispatchThreadID){
 if(any(tid.xy>=uint2(Width,Height)))return;VSOUT i;i.vpos=float4(tid.xy,0,1);i.uv=(float2(tid.xy)+.5)/float2(Width,Height);
 float4 o;NormalsPS(i,o);NormalOut[tid.xy]=o;
}
[numthreads(8,8,1)] void ComposeDiffuseCS(uint3 tid:SV_DispatchThreadID){
 if(any(tid.xy>=uint2(Width,Height)))return;
 float4 original=ColorTex.Load(int3(tid.xy,0));
 if(Mix<=0 && Inspect==0){ColorOut[tid.xy]=original;return;}
 float4 diff=DiffuseTex.Load(int3(tid.xy,0));float3 gi=max(0,ycocg_to_linear(diff.rgb));
 if(Inspect==0 && all(gi==0) && diff.w==1){ColorOut[tid.xy]=original;return;}
 float3 albedo=max(0,AlbedoTex.Load(int3(tid.xy,0)).rgb);
 float3 encoded=Encode(original.rgb);float3 base=unpack_hdr(encoded);
 float3 combined=base*diff.w+gi*albedo;
 // Preserve the pre-exposed linear input when indirect light is zero and AO
 // is neutral. The display-domain pack/unpack pair is only approximate.
 float3 mapped=Decode(pack_hdr(combined));float3 baseline=Decode(pack_hdr(base));
 float3 result=max(0,original.rgb+(mapped-baseline));
 if(Inspect==1)result=Decode(pack_hdr(.4444*diff.w+gi*.4444));
 ColorOut[tid.xy]=float4(Inspect?result:lerp(original.rgb,result,saturate(Mix)),original.a);
}
