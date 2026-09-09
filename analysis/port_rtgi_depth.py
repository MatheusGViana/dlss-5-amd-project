"""Build the first native HLSL stage from the locally supplied Diffuse source.

Development artifact only. Not added to release packages or game hooks.
"""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
source = (root/'rtgi/iMMERSE/MartysMods_RTGI_DIFFUSE.fx').read_text()
sfc = (root/'rtgi/iMMERSE/MartysMods/mmx_sfc.fxh').read_text()

def function(text, name):
    match = re.search(r'^(?:float3|uint2|void)\s+'+name+r'\s*\(', text, re.M)
    if not match:
        raise ValueError(name)
    start = text.index('{', match.start())
    level = 1
    end = start + 1
    while level:
        level += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]

header = source[:source.index('/*=============================================================================', 10)]
prefix = r'''
Texture2D<float4> ColorTex : register(t0);
Texture2D<float> DepthTex : register(t1);
SamplerState ClampLinear : register(s0);
RWTexture2D<float> stNEWGI_Z0 : register(u0);
RWTexture2D<float> stNEWGI_Z1 : register(u1);
RWTexture2D<float> stNEWGI_Z2 : register(u2);
RWTexture2D<float> stNEWGI_Z3 : register(u3);
RWTexture2D<float> stNEWGI_Z4 : register(u4);
cbuffer Host : register(b0) {
 uint Width; uint Height; float FarPlane; uint Reversed;
};
#define BUFFER_PIXEL_SIZE_DLSS (1.0/float2(Width,Height))
struct CSIN {
 uint3 groupthreadid : SV_GroupThreadID;
 uint3 groupid : SV_GroupID;
 uint3 dispatchthreadid : SV_DispatchThreadID;
 uint threadid : SV_GroupIndex;
};
float4 linearize_depth(float4 d) {
 d=Reversed ? 1-d : d;
 return saturate(d / (FarPlane-d*(FarPlane-1)));
}
float depth_to_z(float d) { return d*FarPlane+1; }
// All threads participate in group barriers, including the padded edge tile.
// Guard stores individually rather than returning early from the dispatch.
#define tex2Dstore(target,p,value) { uint tw,th; target.GetDimensions(tw,th); if(all(uint2(p)<uint2(tw,th))) target[uint2(p)]=value; }
'''
helpers = '\n'.join(function(source,n) for n in ['cone_overlap','unpack_hdr'])
helpers += '\n' + function(sfc, 'morton_i_to_xy')
start = source.index('struct ZDownsamplePayload')
end = source.index('/*=============================================================================', start)
stage = source[start:end]
stage = stage.replace('SFC::morton_i_to_xy', 'morton_i_to_xy')
stage = stage.replace('tex2DgatherR(DepthInput, Depth::correct_uv(quad_uv))',
                      'DepthTex.GatherRed(ClampLinear, quad_uv)')
stage = stage.replace('Depth::linearize', 'linearize_depth').replace('Camera::depth_to_z','depth_to_z')
stage = re.sub(r'tex2Dlod\(ColorInput, (.*), 0\)',r'ColorTex.SampleLevel(ClampLinear, \1, 0)',stage)
stage = stage.replace('barrier();','GroupMemoryBarrierWithGroupSync();')
stage = stage.replace('void DownsampleDepthCS(CSIN i)', '[numthreads(16,16,1)] void DownsampleDepthCS(CSIN i)')
out = root/'analysis/rtgi-native'
out.mkdir(exist_ok=True)
(out/'DiffuseDepth.hlsl').write_text(header+prefix+helpers+'\n'+stage)
print(out/'DiffuseDepth.hlsl')
