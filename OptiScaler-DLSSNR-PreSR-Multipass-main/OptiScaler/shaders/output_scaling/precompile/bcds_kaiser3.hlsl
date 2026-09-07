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
        return 1.0f;
    return sin(x) / x;
}

// Modified Bessel function I0 approximation.
static float I0(float x)
{
    float ax = abs(x);
    if (ax < 3.75f)
    {
        float t = x / 3.75f;
        float t2 = t * t;
        return 1.0f
            + t2 * (3.5156229f
            + t2 * (3.0899424f
            + t2 * (1.2067492f
            + t2 * (0.2659732f
            + t2 * (0.0360768f
            + t2 * 0.0045813f)))));
    }
    else
    {
        float t = 3.75f / ax;
        return (exp(ax) / sqrt(ax)) *
            (0.39894228f
            + t * (0.01328592f
            + t * (0.00225319f
            + t * (-0.00157565f
            + t * (0.00916281f
            + t * (-0.02057706f
            + t * (0.02635537f
            + t * (-0.01647633f
            + t * 0.00392377f))))))));
    }
}

static float KaiserWindow(float x, float a, float beta, float invI0Beta)
{
    float ax = abs(x);
    if (ax >= a)
        return 0.0f;

    float r = ax / a;
    float t = sqrt(saturate(1.0f - r * r));
    return I0(beta * t) * invI0Beta;
}

static float Kaiser(float x, float a, float beta, float invI0Beta)
{
    return Sinc(x) * KaiserWindow(x, a, beta, invI0Beta);
}

// Radius 3 => taps at {-2,-1,0,1,2,3} (6 taps) around ip
static const float A_KAISER = 3.0f;

// Good starting point for radius-3 Kaiser in post-TAA pipelines.
static const float BETA = 6.0f;

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

    // For radius 3, use taps centered around ip with offset -2..+3
    int2 base = (int2) ip - int2(2, 2);

    float invI0Beta = 1.0f / I0(BETA);

    float wx[6];
    float wy[6];
    float sumWx = 0.0f;
    float sumWy = 0.0f;

    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        float dx = (float) i - 2.0f - f.x;
        wx[i] = Kaiser(dx, A_KAISER, BETA, invI0Beta);
        sumWx += wx[i];

        float dy = (float) i - 2.0f - f.y;
        wy[i] = Kaiser(dy, A_KAISER, BETA, invI0Beta);
        sumWy += wy[i];
    }

    float invSumWx = (sumWx != 0.0f) ? (1.0f / sumWx) : 0.0f;
    float invSumWy = (sumWy != 0.0f) ? (1.0f / sumWy) : 0.0f;

    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        wx[i] *= invSumWx;
        wy[i] *= invSumWy;
    }

    float2 invSrc = 1.0f / float2((float) _SrcWidth, (float) _SrcHeight);

    float3 acc = 0.0f;

    // Keep clamp; helps with any negative-lobe kernels.
    float3 mn = 1e30f;
    float3 mx = -1e30f;

    [unroll]
    for (int j = 0; j < 6; ++j)
    {
        int y = ClampInt(base.y + j, 0, _SrcHeight - 1);
        float wyj = wy[j];

        [unroll]
        for (int i = 0; i < 6; ++i)
        {
            int x = ClampInt(base.x + i, 0, _SrcWidth - 1);
            float w = wx[i] * wyj;

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
