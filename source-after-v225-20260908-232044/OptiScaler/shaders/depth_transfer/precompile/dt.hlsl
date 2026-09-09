#ifdef VK_MODE
[[vk::binding(0, 0)]]
#endif
Texture2D<float> DepthTex : register(t0);

#ifdef VK_MODE
[[vk::binding(1, 0)]]
#endif
RWTexture2D<float> OutTex : register(u1);

[numthreads(16, 16, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint2 p = tid.xy;
    float d = DepthTex.Load(int3(p, 0));
    OutTex[p] = d;
}
