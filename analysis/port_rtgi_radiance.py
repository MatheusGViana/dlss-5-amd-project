"""Native translation of the supplied diffuse radiance stages; local development."""
from port_rtgi_depth import root, source, header, function
import re
out=root/'analysis/rtgi-native'
harmonics=(root/'rtgi/iMMERSE/MartysMods/mmx_harmonics.fxh').read_text()
sh=harmonics[harmonics.index('namespace SphericalHarmonics'):]
prefix=r'''
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
'''
stage=source[source.index('void InitRadianceVolumeCS'):source.index('struct TraceContext')]
stage=stage.replace('const uint2 target_size = BUFFER_SCREEN_SIZE_DLSS / 4;',
'''const uint2 target_size = BUFFER_SCREEN_SIZE_DLSS / 4;
    if(any(i.dispatchthreadid.xy >= target_size)) return;''',1)
stage=re.sub(r'tex2Dlod\(sNEWGI_Z, ([^,]+), ([^)]+)\)',r'ZTex.SampleLevel(ClampPoint, \1, \2)',stage)
stage=re.sub(r'tex2Dlod\(ColorInput, ([^,]+), ([^)]+)\)',r'ColorTex.SampleLevel(ClampPoint, \1, \2)',stage)
stage=re.sub(r'tex3Dfetch\(stNEWGI_SHVolume, (int3\([^)]*\))\)',r'load_volume(\1)',stage)
for name,size in [('InitRadianceVolumeCS',8),('PropagateRadianceCS0',16),('PropagateRadianceCS1',16),('PropagateRadianceCS2',16),('PropagateRadianceCS3',16)]:
 stage=stage.replace('void '+name+'(',f'[numthreads({size},{size},1)] void '+name+'(')
(out/'DiffuseRadiance.hlsl').write_text(header+prefix+sh+'\n'+function(source,'cone_overlap')+'\n'+function(source,'unpack_hdr')+'\n'+stage)
print(out/'DiffuseRadiance.hlsl')
