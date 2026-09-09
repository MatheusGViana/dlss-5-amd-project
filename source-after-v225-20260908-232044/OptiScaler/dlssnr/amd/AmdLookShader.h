#pragma once
// Spatial formulas adapted from the user-supplied DLSS5_Look.fx. The original
// operates on ReShade's display buffer; this variant uses a reversible bounded
// colour domain around the pre-exposed linear SR input. It is not a neural model.
inline constexpr char AmdLookShader[] = R"(
Texture2D<float4> src : register(t0);
RWTexture2D<float4> dst : register(u0);
cbuffer Controls : register(b0) {
 uint w; uint h; uint Look; uint Inspect;
 float Mix; float MaterialDetail; float ShapeDefinition; float LocalLighting;
 float SkinDetail; float SkinSoftness; float SpecularControl; float HighlightRollOff;
 float ColourSeparation; float ShadowDepth; float AntiHalo; float FlatAreaProtection;
 float Tone; float ExposureEV; float Contrast; float Saturation;
 float HighlightCompression; float PreExposure; uint DetectSkin; uint reserved;
};
float Y(float3 c) { return dot(c,float3(.2126,.7152,.0722)); }
float3 Encode(float3 c) { c=max(c,0)/PreExposure; return pow(c/(1+c),1.0/2.2); }
float3 Decode(float3 c) { c=pow(clamp(c,0,.9999),2.2); return c/max(1-c,.00001)*PreExposure; }
float3 Sample(int2 p) { return Encode(src.Load(int3(clamp(p,int2(0,0),int2(w-1,h-1)),0)).rgb); }
float Skin(float3 c,float y) {
 float cb=(c.b-y)*.539,cr=(c.r-y)*.635;
 return saturate(smoothstep(.012,.055,cr)*(1-smoothstep(.19,.30,cr))*(1-smoothstep(.025,.145,cb))*smoothstep(.05,.17,y)*(1-smoothstep(.86,1,y)));
}
[numthreads(8,8,1)] void main(uint3 tid:SV_DispatchThreadID) {
 if(tid.x>=w || tid.y>=h) return;
 int2 p=tid.xy;
 float4 original=src.Load(int3(p,0));
 if(Mix==0 && Tone==0 && Inspect==0) { dst[p]=original; return; }
 float3 s=Encode(original.rgb),outc=s;
 float y=Y(s),skin=DetectSkin?Skin(s,y):0,residual=0,local=0;
 if(Mix>0 || Inspect!=0) {
  float sum=0,ws=0;
  [unroll]for(int j=-2;j<=2;j++)[unroll]for(int i=-2;i<=2;i++) {
   float yy=Y(Sample(p+int2(i,j)));
   float weight=exp(-abs(yy-y)*18.0)/(1.0+abs(i)+abs(j));sum+=yy*weight;ws+=weight;
  }
  float base=sum/ws;
  float yn=Y(Sample(p+int2(0,-1))),ys=Y(Sample(p+int2(0,1))),ye=Y(Sample(p+int2(1,0))),yw=Y(Sample(p+int2(-1,0)));
  float cross=(yn+ys+ye+yw+4*y)/8;
  float2 d=float2(y-cross,cross-base);
  float lo=min(y,min(min(yn,ys),min(ye,yw))),hi=max(y,max(max(yn,ys),max(ye,yw))),range=max(hi-lo,.0001);
  float cinematic=Look>=2?1:0,strong=Look==3?1:0,natural=Look==1?1:0;
  float fineGain=MaterialDetail*(natural>.5?.68:1)*(1+.25*cinematic+.22*strong);
  float shapeGain=ShapeDefinition*(natural>.5?.72:1)*(1+.30*cinematic+.25*strong);
  float gate=lerp(1,smoothstep(.005,.035,range),FlatAreaProtection);
  float skinGain=lerp(1,.55+.62*SkinDetail,skin);
  residual=(d.x*fineGain*skinGain+d.y*shapeGain)*gate;
  local=(y-base)*LocalLighting*(.20+.12*cinematic+.08*strong)*(1-HighlightRollOff*smoothstep(.72,.98,y));
  float limit=lerp(.10,.018,AntiHalo)+range*lerp(.30,.10,AntiHalo);
  residual=clamp(residual+local,-limit,limit);
  float ny=y+residual;
  float spec=saturate((y-base-.018)*12)*smoothstep(.35,.92,y);
  ny-=spec*SpecularControl*(.010+.018*cinematic)*lerp(1,SkinSoftness,skin);
  float oc=saturate((base-y-.012)*10)*smoothstep(.08,.72,y);
  ny-=oc*ShadowDepth*(.008+.010*cinematic);
  float shoulder=ny/(1+ny*(.035+.045*cinematic+.025*strong)*HighlightRollOff);
  ny=lerp(ny,shoulder,.40+.25*cinematic);
  float mid=1-abs(saturate(ny)*2-1);
  ny+=(ny-.5)*(.025+.065*cinematic+.035*strong-.012*natural)*mid;
  ny=clamp(ny,lerp(0,lo-.012,AntiHalo),lerp(1,hi+.012,AntiHalo));
  outc=s*((ny+1e-5)/(y+1e-5));
  float chroma=1+ColourSeparation*(.045+.045*cinematic)*(1-.55*skin);
  outc=saturate(lerp(ny.xxx,outc,chroma));
 }
 float3 resultColour=Mix>0?lerp(original.rgb,Decode(outc),Mix):original.rgb;
 if(Tone>0) {
  float3 exposed=max(resultColour,0)*exp2(ExposureEV);
  float luminance=Y(exposed)/PreExposure;
  exposed/=1+HighlightCompression*luminance;
  float3 bounded=Encode(exposed);
  bounded=saturate((bounded-.5)*Contrast+.5);
  bounded=saturate(lerp(Y(bounded).xxx,bounded,Saturation));
  resultColour=lerp(resultColour,Decode(bounded),Tone);
 }
 if(Inspect==1) resultColour=Decode(skin.xxx);
 if(Inspect==2) resultColour=Decode((.5+residual*5).xxx);
 if(Inspect==3) resultColour=Decode((.5+local*8).xxx);
 dst[p]=float4(clamp(resultColour,-65504,65504),original.a);
}
)";
