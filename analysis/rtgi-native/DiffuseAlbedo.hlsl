/*=============================================================================
                                                           
 d8b 888b     d888 888b     d888 8888888888 8888888b.   .d8888b.  8888888888 
 Y8P 8888b   d8888 8888b   d8888 888        888   Y88b d88P  Y88b 888        
     88888b.d88888 88888b.d88888 888        888    888 Y88b.      888        
 888 888Y88888P888 888Y88888P888 8888888    888   d88P  "Y888b.   8888888    
 888 888 Y888P 888 888 Y888P 888 888        8888888P"      "Y88b. 888        
 888 888  Y8P  888 888  Y8P  888 888        888 T88b         "888 888        
 888 888   "   888 888   "   888 888        888  T88b  Y88b  d88P 888        
 888 888       888 888       888 8888888888 888   T88b  "Y8888P"  8888888888                                                                 
                                                                            
    Copyright (c) Pascal Gilcher. All rights reserved.
    
    * Unauthorized copying of this file, via any medium is strictly prohibited
 	* Proprietary and confidential

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.

===============================================================================

    Author:         Pascal Gilcher

    More info:      https://martysmods.com
                    https://patreon.com/mcflypg
                    https://github.com/martymcmodding  	

=============================================================================*/



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
float3 cone_overlap(float3 c)
{
    float k = 0.99 * 0.33;
    float2 f = float2(1 - 2 * k, k);
    float3x3 m = float3x3(f.xyy, f.yxy, f.yyx);
    return mul(c, m);
}
float3 unpack_hdr_rtgi(float3 color)
{
    color  = saturate(color);
    color = cone_overlap(color);
    color = color*0.283799*((2.52405+color)*color);    
    //color = srgb_to_AgX(color);
    color = color * rcp(1.04 - saturate(color));    
    return color;
}
float3 sdr_to_hdr(float3 c)
{ 
    return unpack_hdr_rtgi(c);
}
float2 downsample_kuwahara(Texture2D<float4> s0, float2 uv, const bool horizontal)
{
    uint tw,th;s0.GetDimensions(tw,th);const float2 texelsize=rcp(float2(tw,th));  
    float2 axis = horizontal ? float2(texelsize.x, 0) : float2(0, texelsize.y);

    float4 mL = 0;
    float4 mR = 0;
    float2 wsum = 0;

    [unroll]
    for(int j = -11; j <= 11; j++)
    {
        float2 off = j * axis;
        float2 tuv = uv + off;
        float w = exp(-j*j/121.0 * 3.0) * Math::inside_screen(tuv);
            
        float2 t = s0.SampleLevel(ClampLinear,tuv,0).xy;

        w *= j == 0 ? 0.5 : 1;
        mL += float4(t, t * t) * w * (j <= 0);
        mR += float4(t, t * t) * w * (j >= 0);
        wsum += w * float2(j <= 0, j >= 0);
    }

    mL /= wsum.x; 
    mR /= wsum.y;
    float vL = max(0, mL.w - mL.y * mL.y); //.y .w is the regular luma BS so we can use that as weight
    float vR = max(0, mR.w - mR.y * mR.y);
    float2 w = rcp(0.25 + sqrt(float2(vL, vR)));
    return (mL.xy * w.x + mR.xy * w.y) / (w.x + w.y);    
}
void InitAlbedoPyramidPS(in VSOUT i, out float2 o)
{
    float3 hdr = sdr_to_hdr(ColorTex.SampleLevel(ClampLinear,i.uv,0).rgb);
    float loglum = dot(0.3333, log2(max(1e-3, hdr)));
    o.y = loglum; //key to .y  
    o.x = -loglum;
}
float func(float a, float b, float levelnorm)
{
    float res = abs(a - b) / max3(a, b, 1);
    res *= lerp(0.02, 0.3, EQUALIZATION_STRENGTH);    
    return saturate(res / (1 + res));
}
void AlbedoMainPS(in VSOUT i, out float3 o)
{
    float4 m = 0;
    float ws = 0.0;

    [unroll]for(int y = -1; y <= 1; y++) 
    [unroll]for(int x = -1; x <= 1; x++)    
    {
        float2 t = FusedTex.SampleLevel(ClampLinear,i.uv,0,int2(x, y)).xy;
        float w = exp(-(x * x + y * y));
        m += float4(t.y, t.y * t.y, t.y * t.x, t.x) * w;
        ws += w;
    }    

    m /= ws;    
    float a = (m.z - m.x * m.w) / (max(m.y - m.x * m.x, 0.0) + 0.00001);
    float b = m.w - a * m.x;

    float guide = dot(0.3333, ColorTex.SampleLevel(ClampLinear,i.uv,0).rgb);
    float bias = a * guide + b;

	float target = 0.18;
    float3 target_hdr = sdr_to_hdr(target.xxx);
    float target_loglum = dot(0.3333, log2(max(1e-3, target_hdr)));  
    bias += target_loglum; 

    o = bias.x * 0.05 + 0.5;    
    
    o = ColorTex.SampleLevel(ClampLinear,i.uv,0).rgb;
    o = sdr_to_hdr(o);
    
    o *= exp2(bias);  

    //Let L = lighting, A = albedo, C = final scene color, p = multiscatter probability
	//then  C = L * (A + p * A + p² * A ....)
	//this means the final color is a combination of single and multiscattering. I'm fudging things here with a constant light
	//but if we invert this MacLaurin series to get the actual albedo A, we get... a reinhard tonemap curve lmao	
	 
    float3 L = 1; //assumed lighting
	float3 C = o.rgb;
	float p = 0.5; //backscatter probability, lambert we assume 0.5
	//o.rgb = C / (L + C * p);

    //if(tempF1.x > 0)
	{
       float3 reverse_multiscattered = C / (L + C * p);
       o = normalize(reverse_multiscattered + 1e-3) * length(o); //normalize to get the original color
	}
}
float4 sample2D_bspline(Texture2D<float4> s, float2 iuv, int2 size)
{
    float4 uv;
	uv.xy = iuv * size;

    float2 center = floor(uv.xy - 0.5) + 0.5;
	float4 d = float4(uv.xy - center, 1 + center - uv.xy);
	float4 d2 = d * d;
	float4 d3 = d2 * d;

    float4 o = d2 * 0.12812 + d3 * 0.07188; //approx |err|*255 < 0.2 < bilinear precision
	uv.xy = center - o.zw;
	uv.zw = center + 1 + o.xy;
	uv /= size.xyxy;

    float4 w = 0.16666666 + d * 0.5 + 0.5 * d2 - d3 * 0.3333333;
	w = w.wwyy * w.zxzx;

    return w.x * s.SampleLevel(ClampLinear,uv.xy,0)
	     + w.y * s.SampleLevel(ClampLinear,uv.zy,0)
		 + w.z * s.SampleLevel(ClampLinear,uv.xw,0)
		 + w.w * s.SampleLevel(ClampLinear,uv.zw,0);
}
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
