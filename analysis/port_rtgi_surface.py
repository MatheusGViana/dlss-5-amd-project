"""Native surface normals and diffuse composition from supplied local sources."""
from port_rtgi_trace import root, source, header, fn
out=root/'analysis/rtgi-native'
launchpad=(root/'rtgi/iMMERSE/MartysMods_LAUNCHPAD.fx').read_text()
prefix=r'''
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
'''
normals=fn(launchpad,'NormalsPS').replace('out float4 o : SV_Target0','out float4 o')
normals=normals.replace('rcp(dot(temp_normal, temp_normal))','rcp(max(dot(temp_normal, temp_normal),1e-20))')
normals=normals.replace('o = Math::octahedral_enc(-normal).xyxy;', 'o = float4(dot(normal,normal)>0.5 ? normal : float3(0,0,-1),1);')
normals=normals.replace('normal *= rsqrt(dot(normal, normal) + 1e-8);', 'normal /= max(max(abs(normal.x),max(abs(normal.y),abs(normal.z))),1e-30); normal *= rsqrt(max(dot(normal,normal),1e-20));')
helpers='\n'.join(fn(source,n) for n in ['cone_overlap','cone_overlap_inv','unpack_hdr','pack_hdr','ycocg_to_linear'])
wrappers=r'''
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
'''
(out/'DiffuseSurface.hlsl').write_text(header+prefix+helpers+'\n'+normals+wrappers)
print(out/'DiffuseSurface.hlsl')
