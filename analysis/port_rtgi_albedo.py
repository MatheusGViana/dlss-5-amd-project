"""Translate the supplied albedo preparation; no optical-flow replacement."""
from port_rtgi_temporal import root, header, fn, replace_calls
out=root/'analysis/rtgi-native';inc=root/'rtgi/iMMERSE/MartysMods'
s=(root/'rtgi/iMMERSE/MartysMods_LAUNCHPAD.fx').read_text()
prefix=r'''
Texture2D<float4> ColorTex:register(t0);
Texture2D<float4> InputTex:register(t1);
Texture2D<float4> PyramidTex[10]:register(t2);
Texture2D<float4> FusedTex:register(t12);
RWTexture2D<float4> OutputTex:register(u0);
SamplerState ClampLinear:register(s0);
cbuffer Host:register(b0){uint Width;uint Height;uint LowestLevel;uint Horizontal;};
#define EQUALIZATION_STRENGTH 1.0
struct VSOUT{float4 vpos;float2 uv;};
float max3(float a,float b,float c){return max(a,max(b,c));}
namespace Math{bool inside_screen(float2 uv){return all(uv>=0)&&all(uv<=1);}}
'''
code='\n'.join(fn(s,n) for n in ['cone_overlap','unpack_hdr_rtgi','sdr_to_hdr','downsample_kuwahara','InitAlbedoPyramidPS','func','AlbedoMainPS'])
code=code.replace('const sampler s0','Texture2D<float4> s0').replace('const float2 texelsize = rcp(tex2Dsize(s0, 0));','uint tw,th;s0.GetDimensions(tw,th);const float2 texelsize=rcp(float2(tw,th));')
code=code.replace(' : SV_Target0','')
code+='\n'+fn((inc/'mmx_texture.fxh').read_text(),'sample2D_bspline').replace('sampler s','Texture2D<float4> s')
textures={'s0':'s0','s':'s','ColorInput':'ColorTex','sFusedAlbedoPyramid':'FusedTex'}
code=replace_calls(code,'tex2Dlod',lambda a:textures[a[0]]+'.SampleLevel(ClampLinear,'+a[1]+','+a[2]+')')
code=replace_calls(code,'tex2D',lambda a:textures[a[0]]+'.SampleLevel(ClampLinear,'+a[1]+',0'+(','+a[2] if len(a)>2 else '')+')')
wrappers=r'''
VSOUT pixel(uint2 p){VSOUT i;i.vpos=float4(p,0,1);i.uv=(float2(p)+.5)/float2(Width,Height);return i;}
[numthreads(8,8,1)] void InitAlbedoCS(uint3 p:SV_DispatchThreadID){if(any(p.xy>=uint2(Width,Height)))return;float2 o;InitAlbedoPyramidPS(pixel(p.xy),o);OutputTex[p.xy]=float4(o,0,0);}
[numthreads(8,8,1)] void BlurAlbedoCS(uint3 p:SV_DispatchThreadID){if(any(p.xy>=uint2(Width,Height)))return;float2 o=downsample_kuwahara(InputTex,pixel(p.xy).uv,Horizontal!=0);OutputTex[p.xy]=float4(o,0,0);}
[numthreads(8,8,1)] void FuseAlbedoCS(uint3 p:SV_DispatchThreadID){
 if(any(p.xy>=uint2(Width,Height)))return;float2 uv=pixel(p.xy).uv;float2 G[10];
 [unroll]for(uint j=0;j<10;++j){uint w,h;PyramidTex[j].GetDimensions(w,h);G[j]=j<4?PyramidTex[j].SampleLevel(ClampLinear,uv,0).xy:sample2D_bspline(PyramidTex[j],uv,int2(w,h)).xy;}
 float2 bias=G[min(LowestLevel,9u)];
 [unroll]for(int k=8;k>=0;--k)if(k<int(LowestLevel))bias=lerp(bias,G[k],func(G[k].y,bias.y,float(k)/max(1,LowestLevel)));
 OutputTex[p.xy]=float4(bias.x,dot(.3333,ColorTex.SampleLevel(ClampLinear,uv,0).rgb),0,0);
}
[numthreads(8,8,1)] void ResolveAlbedoCS(uint3 p:SV_DispatchThreadID){if(any(p.xy>=uint2(Width,Height)))return;float3 o;AlbedoMainPS(pixel(p.xy),o);OutputTex[p.xy]=float4(o,1);}
'''
(out/'DiffuseAlbedo.hlsl').write_text(header+prefix+code+wrappers)
print(out/'DiffuseAlbedo.hlsl')
