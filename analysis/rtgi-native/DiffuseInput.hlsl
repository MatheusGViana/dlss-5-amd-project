Texture2D<float4> ColorTex:register(t0);
Texture2D<float> DepthTex:register(t1);
Texture2D<float2> MotionTex:register(t2);
RWTexture2D<float4> EncodedOut:register(u0);
RWTexture2D<float> DepthOut:register(u1);
RWTexture2D<float2> MotionOut:register(u2);
cbuffer Host:register(b0){uint Width;uint Height;float PreExposure;float ScaleX;float ScaleY;uint Pad0;uint Pad1;uint Pad2;};
[numthreads(8,8,1)] void PrepareInputCS(uint3 p:SV_DispatchThreadID){
 if(any(p.xy>=uint2(Width,Height)))return;
 float3 c=max(ColorTex.Load(int3(p.xy,0)).rgb,0)/PreExposure;
 EncodedOut[p.xy]=float4(pow(c/(1+c),1.0/2.2),1);
 DepthOut[p.xy]=DepthTex.Load(int3(p.xy,0));
 MotionOut[p.xy]=MotionTex.Load(int3(p.xy,0))*float2(ScaleX/Width,ScaleY/Height);
}
