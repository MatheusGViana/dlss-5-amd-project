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

static int ClampInt(int v, int lo, int hi)
{
    return min(max(v, lo), hi);
}

static float Sinc(float x)
{
    x *= 3.1415926535f;
    if (abs(x) < 1e-5f)
    {
        return 1.0f;
    }
    return sin(x) / x;
}

static float Lanczos(float x, float a)
{
    float ax = abs(x);
    if (ax >= a)
    {
        return 0.0f;
    }
    return Sinc(x) * Sinc(x / a);
}

// Lanczos3: a = 3, radius 3 => 6 taps per axis
static const float A_LANCZOS = 3.0f;
static const int TAP_COUNT = 6;
static const int RADIUS = 3;

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint ox = id.x;
    uint oy = id.y;
    if (ox >= (uint) _DstWidth || oy >= (uint) _DstHeight)
    {
        return;
    }

    float2 dst = float2((float) ox + 0.5f, (float) oy + 0.5f);
    float2 scale = float2((float) _SrcWidth / (float) _DstWidth,
                          (float) _SrcHeight / (float) _DstHeight);

    float2 srcPos = dst * scale - 0.5f;

    float2 ip = floor(srcPos);
    float2 f = srcPos - ip;

    // For radius 3, use taps at {-2,-1,0,1,2,3} around ip
    int2 base = (int2) ip - int2(RADIUS - 1, RADIUS - 1); // ip - 2

    float wx[TAP_COUNT];
    float wy[TAP_COUNT];
    float sumWx = 0.0f;
    float sumWy = 0.0f;

    [unroll]
    for (int i = 0; i < TAP_COUNT; ++i)
    {
        float dx = (float) i - (float) (RADIUS - 1) - f.x; // i - 2 - f.x
        wx[i] = Lanczos(dx, A_LANCZOS);
        sumWx += wx[i];

        float dy = (float) i - (float) (RADIUS - 1) - f.y; // i - 2 - f.y
        wy[i] = Lanczos(dy, A_LANCZOS);
        sumWy += wy[i];
    }

    float invSumWx = (sumWx != 0.0f) ? (1.0f / sumWx) : 0.0f;
    float invSumWy = (sumWy != 0.0f) ? (1.0f / sumWy) : 0.0f;

    [unroll]
    for (int i = 0; i < TAP_COUNT; ++i)
    {
        wx[i] *= invSumWx;
        wy[i] *= invSumWy;
    }

    float2 invSrc = 1.0f / float2((float) _SrcWidth, (float) _SrcHeight);

    float3 acc = 0.0f;

    // Min/max clamp (Lanczos rings more than bicubic)
    float3 mn = 1e30;
    float3 mx = -1e30;

    [unroll]
    for (int j = 0; j < TAP_COUNT; ++j)
    {
        int y = ClampInt(base.y + j, 0, _SrcHeight - 1);
        float wyj = wy[j];

        [unroll]
        for (int i = 0; i < TAP_COUNT; ++i)
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
