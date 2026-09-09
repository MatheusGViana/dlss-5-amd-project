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
    float2 _Scale;
    float2 _InvScale;
    float2 _SrcOffset;
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

static int ClampInt(int v, int lo, int hi)
{
    return min(max(v, lo), hi);
}

static float Sinc(float x)
{
    x *= 3.1415926535f;
    if (abs(x) < 1e-5f)
        return 1.0f;
    return sin(x) / x;
}

static float Lanczos(float x, float a)
{
    float ax = abs(x);
    if (ax >= a)
        return 0.0f;
    return Sinc(x) * Sinc(x / a);
}

// Set a=2 for Lanczos2, a=3 for Lanczos3 (but then you need 6x6 or bigger).
static const float A_LANCZOS = 2.0f;

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

    float2 srcPos = dst * scale - 0.5f;

    float2 ip = floor(srcPos);
    float2 f = srcPos - ip;

    // For radius 2, use taps at {-1,0,1,2} around ip
    int2 base = (int2) ip - int2(1, 1);

    float wx[4];
    float wy[4];
    float sumWx = 0.0f;
    float sumWy = 0.0f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float dx = (float) i - 1.0f - f.x;
        wx[i] = Lanczos(dx, A_LANCZOS);
        sumWx += wx[i];

        float dy = (float) i - 1.0f - f.y;
        wy[i] = Lanczos(dy, A_LANCZOS);
        sumWy += wy[i];
    }

    float invSumWx = (sumWx != 0.0f) ? (1.0f / sumWx) : 0.0f;
    float invSumWy = (sumWy != 0.0f) ? (1.0f / sumWy) : 0.0f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        wx[i] *= invSumWx;
        wy[i] *= invSumWy;
    }

    float2 invSrc = 1.0f / float2((float) _SrcWidth, (float) _SrcHeight);

    float3 acc = 0.0f;

    // Clamp like you did for bicubic is recommended (Lanczos rings more)
    float3 mn = 1e30;
    float3 mx = -1e30;

    [unroll]
    for (int j = 0; j < 4; ++j)
    {
        int y = ClampInt(base.y + j, 0, _SrcHeight - 1);
        float wyj = wy[j];

        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            int x = ClampInt(base.x + i, 0, _SrcWidth - 1);
            float w = wx[i] * wyj;

            // Sample at exact texel centers via UV
            float2 uv = (float2((float) x + 0.5f, (float) y + 0.5f)) * invSrc;
            float3 s = InputTexture.SampleLevel(LinearClampSampler, uv, 0.0f).rgb;

            mn = min(mn, s);
            mx = max(mx, s);

            acc += s * w;
        }
    }

    float3 outRgb = clamp(acc, mn, mx);
    OutputTexture[uint2(ox, oy)] = float4(outRgb, 1.0f);
}