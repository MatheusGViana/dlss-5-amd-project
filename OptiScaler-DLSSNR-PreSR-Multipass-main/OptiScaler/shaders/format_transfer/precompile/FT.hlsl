Texture2D<float4> SourceTexture : register(t0);
RWTexture2D<float4> DestinationTexture : register(u0);

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float4 c = SourceTexture.Load(int3(dispatchThreadID.xy, 0));
    c.a = 1.0f;
    DestinationTexture[dispatchThreadID.xy] = c;
}