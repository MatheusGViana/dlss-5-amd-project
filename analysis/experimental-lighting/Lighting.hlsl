// Original screen-space local bounce and occlusion implementation.
// No external shader includes, lookup textures, or translated effect code.
cbuffer Params : register(b0) {
 uint W,H,Reverse,Quality;
 float FarPlane,TanHalf,Thickness,Fade;
 float Lighting,Occlusion,Ambient,Mix;
 uint Inspect,Denoiser; float Smoothness,Pad;
 float Contact,Saturation,Radius,Pad2;
};
Texture2D<float4> Colour : register(t0);
Texture2D<float> Depth : register(t1);
Texture2D<float4> Signal : register(t2);
RWTexture2D<float4> Result : register(u0);
int2 bounded(int2 p) { return clamp(p,int2(0,0),int2(W-1,H-1)); }
float distanceAt(int2 p) {
 float d=saturate(Depth.Load(int3(bounded(p),0)));
 d=Reverse ? 1-d : d;
 return 1.0/max(1e-6,1-d*(1-1/FarPlane));
}
float3 positionAt(int2 p) {
 float z=distanceAt(p);
 float2 xy=(float2(p)+0.5)/float2(W,H)*2-1;
 return float3(xy*float2((float)W/H,-1)*TanHalf*z,z);
}
[numthreads(8,8,1)]
void GatherCS(uint3 tid:SV_DispatchThreadID) {
 if(tid.x>=W||tid.y>=H)return;
 int2 p=tid.xy;
 float3 center=positionAt(p);
 // Choose the shorter derivative to avoid normals spanning depth discontinuities.
 float3 a=positionAt(bounded(p+int2(1,0)))-center;
 float3 b=center-positionAt(bounded(p-int2(1,0)));
 float3 c=positionAt(bounded(p+int2(0,1)))-center;
 float3 d=center-positionAt(bounded(p-int2(0,1)));
 float3 n=cross(dot(a,a)<dot(b,b)?a:b,dot(c,c)<dot(d,d)?c:d);
 n*=rsqrt(max(dot(n,n),1e-20));
 if(dot(n,center)>0)n=-n;
 float3 bounce=0; float blocked=0;
 uint count=8+Quality*8;
 float radius=max(2.0,min(W,H)*(0.012+0.07*Thickness)*Radius);
 [loop]for(uint i=0;i<count;i++) {
  // Deterministic spiral: no frame-varying noise or external lookup assets.
  float fraction=(i+0.5)/count;
  float angle=i*2.39996323;
  int2 q=p+int2(round(float2(cos(angle),sin(angle))*radius*sqrt(fraction)));
  if(any(q<0)||q.x>=W||q.y>=H)continue;
  float3 delta=positionAt(q)-center;
  float lengthSq=dot(delta,delta);
  float facing=saturate(dot(n,delta)*rsqrt(max(lengthSq,1e-12))-0.025);
  float rangeWeight=saturate(1-sqrt(lengthSq)/max(center.z*(0.1+Thickness),0.001));
  float weight=facing*rangeWeight;
  bounce+=max(Colour.Load(int3(q,0)).rgb,0)*weight;
  blocked+=weight;
 }
 float fade=1-smoothstep(Fade*FarPlane,FarPlane,center.z);
 Result[p]=float4(bounce/count*fade,blocked/count*fade);
}
[numthreads(8,8,1)]
void ResolveCS(uint3 tid:SV_DispatchThreadID) {
 if(tid.x>=W||tid.y>=H)return;
 int2 p=tid.xy;
 float4 src=Colour.Load(int3(p,0));
 if(Mix<=0 || (Lighting==0 && Occlusion==0 && Ambient==1 && Contact==0 && Inspect==0)) {
  Result[p]=src; return;
 }
 float4 filtered=0; float total=0; float z=distanceAt(p);
 int radius=Denoiser==0?0:(Denoiser==1?1:2);
 [loop]for(int y=-radius;y<=radius;y++) [loop]for(int x=-radius;x<=radius;x++) {
  int2 q=bounded(p+int2(x,y));
  float dz=abs(distanceAt(q)-z)/max(z,1e-5);
  float weight=exp(-dz*(100-80*Smoothness)-0.5*(x*x+y*y));
  filtered+=Signal.Load(int3(q,0))*weight; total+=weight;
 }
 filtered/=max(total,1e-6);
 // Short-range depth cavity shading. It is nondirectional, not a light-source shadow map.
 float cavity=0;
 if(Contact>0) {
  [unroll]for(int j=0;j<8;j++) {
   float angle=j*0.78539816;
   int2 q=bounded(p+int2(round(float2(cos(angle),sin(angle))*2)));
   float relative=(z-distanceAt(q))/max(z,1e-5);
   cavity+=saturate(relative*40-0.02)*saturate(1-relative*4);
  }
 }
 float contactVisibility=1-saturate(cavity*Contact/8);
 float luminance=dot(filtered.rgb,float3(0.2126,0.7152,0.0722));
 filtered.rgb=max(0,lerp(luminance.xxx,filtered.rgb,Saturation));
 float visibility=saturate(1-filtered.a*Occlusion);
 float3 base=max(src.rgb,0);
 float3 tint=base/(1+base);
 float3 lit=base*lerp(Ambient,1,visibility)*visibility*contactVisibility+filtered.rgb*tint*Lighting;
 if(Inspect!=0)lit=filtered.rgb*Lighting+visibility*0.15;
 Result[p]=float4(min(lerp(src.rgb,lit,Mix),65504),src.a);
}
