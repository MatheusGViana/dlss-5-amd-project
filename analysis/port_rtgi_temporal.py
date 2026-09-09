"""Native compute translation of diffuse moments, reprojection and filtering."""
from port_rtgi_trace import root, source, header, fn
import re
out=root/'analysis/rtgi-native';inc=root/'rtgi/iMMERSE/MartysMods'
prefix=r'''
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
'''
math=(inc/'mmx_math.fxh').read_text()
helpers='namespace Math { bool inside_screen(float2 uv){return all(uv>=0)&&all(uv<=1);} float2 fast_sign(float2 v){return v>=0?1:-1;}\n'+fn(math,'octahedral_enc')+'\n'+fn(math,'octahedral_dec')+'\n}\n'
helpers+='namespace SFC {\n'+fn((inc/'mmx_sfc.fxh').read_text(),'morton_i_to_xy')+'\n}\n'
for name in ['pixel_idx_to_uv','get_fade_factor','can_earlyout']:helpers+=fn(source,name)+'\n'
stage=source[source.index('groupshared float4 moments_tgsm'):source.index('void DenoisePS0')]
stage=stage.replace('barrier()','GroupMemoryBarrierWithGroupSync()')
stage=stage.replace('bool inside_screen = Math::inside_screen(prev_uv);','bool inside_screen = HistoryValid && Math::inside_screen(prev_uv);')
stage=stage.replace('float4 prev_diff = tex2D(sNEWGI_AccumDiff, prev_uv);','float4 prev_diff = HistoryValid ? tex2D(sNEWGI_AccumDiff, prev_uv) : 0;')
textures={'sNEWGI_Aux0':'CurrentTex','sNEWGI_AccumDiff':'HistoryTex','sNEWGI_PrevTemporalData':'PreviousDataTex','sNEWGI_SpatialMoments':'MomentsTex','sNEWGI_Aux1':'SignalTex','sNEWGI_Aux3':'GBufferTex','s_diff':'s_diff','s_gbuf':'s_gbuf'}
# Balanced parser preserves nested expressions in texture coordinates.
def replace_calls(text,name,callback):
 cursor=0
 while True:
  a=text.find(name+'(',cursor)
  if a<0:return text
  start=a+len(name)+1;b=start;level=1
  while level:
   level+=(text[b]=='(')-(text[b]==')');b+=1
  body=text[start:b-1];args=[];last=0;depth=0
  for j,c in enumerate(body):
   depth+=(c=='(')-(c==')')
   if c==',' and depth==0:args.append(body[last:j].strip());last=j+1
  args.append(body[last:].strip());value=callback(args)
  text=text[:a]+value+text[b:];cursor=a+len(value)
for channel,method in [('R','GatherRed'),('G','GatherGreen'),('B','GatherBlue'),('A','GatherAlpha')]:
 stage=replace_calls(stage,'tex2Dgather'+channel,lambda a,m=method:textures[a[0]]+'.'+m+'(ClampLinear,'+a[1]+')')
stage=replace_calls(stage,'tex2Dfetch',lambda a:textures[a[0]]+'.Load(int3('+a[1]+',0))')
stage=replace_calls(stage,'tex2D',lambda a:textures[a[0]]+'.SampleLevel(ClampLinear,'+a[1]+',0)')
stage=stage.replace('sampler s_diff','Texture2D<float4> s_diff').replace('sampler s_gbuf','Texture2D<float4> s_gbuf')
stage=stage.replace('if(it > (DENOISER_Q * 2 + 1)) discard;', 'if(it > (DENOISER_Q * 2 + 1)){filter_out.t0=s_diff.Load(int3(center_texel,0));filter_out.t1=s_gbuf.Load(int3(center_texel,0));return;}')
for name in ['SpatialMomentsCS','UpdateHistoryCS']:stage=stage.replace('void '+name+'(', '[numthreads(8,8,1)] void '+name+'(')
wrappers=r'''
[numthreads(8,8,1)] void TemporalReprojectionCS(CSIN input){
 uint2 p=input.dispatchthreadid.xy;if(any(p>=uint2(Width,Height)))return;
 VSOUT i;i.vpos=float4(p,0,1);i.uv=(float2(p)+.5)/float2(Width,Height);PSOUT2 o;
 TemporalReprojectionPS(i,o);SignalOut[p]=o.t0;GBufferOut[p]=o.t1;
}
[numthreads(8,8,1)] void DenoiseCS(CSIN input){
 uint2 p=input.dispatchthreadid.xy;if(any(p>=uint2(Width,Height)))return;
 PSOUT2 o;atrous_pass(p,SignalTex,GBufferTex,Iteration,o);SignalOut[p]=o.t0;GBufferOut[p]=o.t1;
}
'''
(out/'DiffuseTemporal.hlsl').write_text(header+prefix+helpers+stage+wrappers)
print(out/'DiffuseTemporal.hlsl')
