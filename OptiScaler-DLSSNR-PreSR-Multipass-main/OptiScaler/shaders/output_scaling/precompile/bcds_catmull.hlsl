#ifdef VK_MODE
cbuffer Params : register(b0, space0)
#else
cbuffer Params : register(b0)
#endif
{
    int _SrcWidth;
    int _SrcHeight;
    int _DstWidth;
    int _DstHeight;
};

#ifdef VK_MODE
[[vk::binding(1, 0)]]
#endif
Texture2D<float4> InputTexture : register(t0);

#ifdef VK_MODE
[[vk::binding(2, 0)]]
#endif
RWTexture2D<float4> OutputTexture : register(u0);

#ifdef VK_MODE
[[vk::binding(3, 0)]]
#endif
SamplerState LinearClampSampler : register(s0);

// Catmull–Rom = Keys bicubic with A = -0.5
static const float A = -0.5f;

static float CubicKeys(float x)
{
    x = abs(x);
    float x2 = x * x;
    float x3 = x2 * x;

    if (x < 1.0f)
    {
        return (A + 2.0f) * x3 - (A + 3.0f) * x2 + 1.0f;
    }
    else if (x < 2.0f)
    {
        return A * x3 - 5.0f * A * x2 + 8.0f * A * x - 4.0f * A;
    }
    return 0.0f;
}

// Convert 4 cubic taps into 2 bilinear taps on an axis.
static void BicubicAxis(float t, out float w01, out float w23, out float o01, out float o23)
{
    float w0 = CubicKeys(1.0f + t);
    float w1 = CubicKeys(t);
    float w2 = CubicKeys(1.0f - t);
    float w3 = CubicKeys(2.0f - t);

    w01 = w0 + w1;
    w23 = w2 + w3;

    float invW01 = (w01 != 0.0f) ? (1.0f / w01) : 0.0f;
    float invW23 = (w23 != 0.0f) ? (1.0f / w23) : 0.0f;

    o01 = (-1.0f) + (w1 * invW01);
    o23 = (1.0f) + (w3 * invW23);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint ox = id.x;
    uint oy = id.y;
    if (ox >= (uint) _DstWidth || oy >= (uint) _DstHeight)
        return;

    float2 dst = float2((float) ox + 0.5f, (float) oy + 0.5f);
    float2 scale = float2((float) _SrcWidth / (float) _DstWidth,
                          (float) _SrcHeight / (float) _DstHeight);

    float2 srcPos = dst * scale - 0.45f;

    float2 ip = floor(srcPos);
    float2 f = srcPos - ip;

    float2 base = ip - 1.0f;

    float wx01, wx23, ox01, ox23;
    float wy01, wy23, oy01, oy23;
    BicubicAxis(f.x, wx01, wx23, ox01, ox23);
    BicubicAxis(f.y, wy01, wy23, oy01, oy23);

    float2 invSrc = 1.0f / float2((float) _SrcWidth, (float) _SrcHeight);

    float2 uv00 = (base + float2(ox01, oy01) + 0.5f) * invSrc;
    float2 uv10 = (base + float2(ox23, oy01) + 0.5f) * invSrc;
    float2 uv01 = (base + float2(ox01, oy23) + 0.5f) * invSrc;
    float2 uv11 = (base + float2(ox23, oy23) + 0.5f) * invSrc;

    float3 s00 = InputTexture.SampleLevel(LinearClampSampler, uv00, 0.0f).rgb;
    float3 s10 = InputTexture.SampleLevel(LinearClampSampler, uv10, 0.0f).rgb;
    float3 s01 = InputTexture.SampleLevel(LinearClampSampler, uv01, 0.0f).rgb;
    float3 s11 = InputTexture.SampleLevel(LinearClampSampler, uv11, 0.0f).rgb;

    float3 outRgb =
        (s00 * wx01 + s10 * wx23) * wy01 +
        (s01 * wx01 + s11 * wx23) * wy23;

    // Cheap clamp (recommended for HDR stability)
    float3 mn = min(min(s00, s10), min(s01, s11));
    float3 mx = max(max(s00, s10), max(s01, s11));
    outRgb = clamp(outRgb, mn, mx);

    OutputTexture[uint2(ox, oy)] = float4(outRgb, 1.0f);
}