"""Translate the supplied diffuse trace to native D3D12 bindings."""
from port_rtgi_depth import root, source, header
import re
out=root/'analysis/rtgi-native'
def fn(text,name):
 m=re.search(r'^(?:float[234]?|uint[234]?|bool|void)\s+'+name+r'\s*\(',text,re.M)
 if not m: raise ValueError(name)
 a=text.index('{',m.start());b=a+1;level=1
 while level:
  level+=(text[b]=='{')-(text[b]=='}');b+=1
 return text[m.start():b]
inc=root/'rtgi/iMMERSE/MartysMods'
harmonics=(inc/'mmx_harmonics.fxh').read_text();harmonics=harmonics[harmonics.index('namespace SphericalHarmonics'):]
prefix=r'''
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
'''
helpers='namespace Hash {\n'+fn((inc/'mmx_hash.fxh').read_text(),'uhash')+'\n'+fn((inc/'mmx_hash.fxh').read_text(),'uint_to_unorm')+'\n}\n'
helpers+='namespace QMC {\n'+fn((inc/'mmx_qmc.fxh').read_text(),'roberts1')+'\n}\n'
for name in ['pixel_idx_to_uv','check_boundaries','linear_to_ycocg','get_fade_factor','can_earlyout']:helpers+=fn(source,name)+'\n'
stage=source[source.index('struct TraceContext'):source.index('groupshared float4 moments_tgsm')]
for old,new in [('sNEWGI_STBN128_s','PermutationTex'),('sNEWGI_STBN128','NoiseTex')]:
 stage=re.sub(r'tex2Dfetch\('+old+r', ([^)]+)\)',r''+new+r'.Load(int3(\1,0))',stage)
for old,new in [('sNEWGI_Z','ZTex'),('sNEWGI_HorizonLUT','HorizonTex')]:
 # Horizon coordinates contain a comma inside float2; match the final mip argument.
 stage=re.sub(r'tex2Dlod\('+old+r', (.*), ([^,]+)\)\.x',new+r'.SampleLevel(ClampLinear, \1, \2)',stage)
stage=re.sub(r'tex3Dlod\(sNEWGI_SHVolume, float4\((.*), 0\)\)',r'RadianceTex.SampleLevel(ClampLinear,float3(\1),0)',stage)
stage=stage.replace('ZTex.SampleLevel(ClampLinear', 'ZTex.SampleLevel(ClampPoint').replace('RadianceTex.SampleLevel(ClampLinear', 'RadianceTex.SampleLevel(ClampPoint')
stage=stage.replace('tex2Dfetch(stNEWGI_STSGCache, pdf_storage_pos).x','stNEWGI_STSGCache[pdf_storage_pos]')
stage=stage.replace('barrier()', 'GroupMemoryBarrierWithGroupSync()')
stage=stage.replace('void TraceWrapCubicCS(', '[numthreads(32,32,1)] void TraceWrapCubicCS(')
# Cache allocation must be padded to whole 32x32 groups. Output stores are guarded,
# but every padded thread must reach every barrier and retain a unique permutation.
(out/'DiffuseTrace.hlsl').write_text(header+prefix+harmonics+'\n'+helpers+stage)
print(out/'DiffuseTrace.hlsl')
