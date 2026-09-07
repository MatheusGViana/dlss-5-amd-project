// Headless shader test: Windows D3D11 WARP executes the shared HLSL, no game or NR DLL.
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <cstddef>
#include "../OptiScaler/shaders/dlssnr/DlssNr_Common.h"
using Microsoft::WRL::ComPtr;
struct Pixel { float r, g, b, a; };
static void check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("D3D call failed"); }
static void expect(bool ok, const char* label) { if (!ok) throw std::runtime_error(label); }
static bool same(Pixel a, Pixel b) {
    return std::abs(a.r-b.r)<0.0001f && std::abs(a.g-b.g)<0.0001f && std::abs(a.b-b.b)<0.0001f && a.a==b.a;
}
int wmain(int argc, wchar_t** argv) try {
    if (argc != 2) throw std::runtime_error("Pass the dlssnr.hlsl path");
    static_assert(offsetof(DlssNrConstants, SkinProtection) == 92);
    static_assert(offsetof(DlssNrConstants, EnvironmentColour) == 112);
    ComPtr<ID3DBlob> code, errors;
    HRESULT compiled = D3DCompileFromFile(argv[1], nullptr, nullptr, "CSMain", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (errors) std::fprintf(stderr, "%s", (char*)errors->GetBufferPointer());
    check(compiled);
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> ctx;
    check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &ctx));
    ComPtr<ID3D11ComputeShader> shader;
    check(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader));
    const std::array<Pixel, 2> base {{{0.75f,0.50f,0.40f,1}, {0.20f,0.40f,0.80f,1}}};
    const std::array<Pixel, 2> edited {{{0.50f,0.75f,0.40f,1}, {0.65f,0.25f,0.40f,1}}};
    D3D11_TEXTURE2D_DESC desc {};
    desc.Width=2; desc.Height=1; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> original, model, output, keep, readback;
    D3D11_SUBRESOURCE_DATA data {base.data(), sizeof(base), 0};
    check(device->CreateTexture2D(&desc,&data,&original));
    data.pSysMem=edited.data(); check(device->CreateTexture2D(&desc,&data,&model));
    ComPtr<ID3D11ShaderResourceView> originalSrv, modelSrv;
    check(device->CreateShaderResourceView(original.Get(),nullptr,&originalSrv));
    check(device->CreateShaderResourceView(model.Get(),nullptr,&modelSrv));
    desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    check(device->CreateTexture2D(&desc,nullptr,&output)); check(device->CreateTexture2D(&desc,nullptr,&keep));
    ComPtr<ID3D11UnorderedAccessView> outputUav, keepUav;
    check(device->CreateUnorderedAccessView(output.Get(),nullptr,&outputUav));
    check(device->CreateUnorderedAccessView(keep.Get(),nullptr,&keepUav));
    desc.BindFlags=0; desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(device->CreateTexture2D(&desc,nullptr,&readback));
    D3D11_BUFFER_DESC buffer {}; buffer.ByteWidth=sizeof(DlssNrConstants); buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> constants; check(device->CreateBuffer(&buffer,nullptr,&constants));
    D3D11_SAMPLER_DESC sampling {}; sampling.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampling.AddressU=sampling.AddressV=sampling.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP; sampling.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler; check(device->CreateSamplerState(&sampling,&sampler));
    ID3D11ShaderResourceView* srvs[]={originalSrv.Get(),modelSrv.Get(),originalSrv.Get(),originalSrv.Get(),originalSrv.Get()};
    ID3D11UnorderedAccessView* uavs[]={outputUav.Get(),keepUav.Get()};
    ctx->CSSetShader(shader.Get(),nullptr,0); ctx->CSSetShaderResources(0,5,srvs);
    ctx->CSSetUnorderedAccessViews(0,2,uavs,nullptr); ctx->CSSetConstantBuffers(0,1,constants.GetAddressOf());
    ctx->CSSetSamplers(0,1,sampler.GetAddressOf());
    DlssNrConstants settings {}; settings.Mode=DlssNrMode_Resolve; settings.Width=2; settings.Height=1;
    settings.WhitePoint=1; settings.Passthrough=1; settings.ApplyModel=1; settings.ReversibleMode=2;
    settings.TransferStrength=settings.ColourStrength=1; settings.MaxRatio=2;
    settings.SkinDetail=settings.SkinColour=settings.EnvironmentDetail=settings.EnvironmentColour=1;
    auto run=[&]() {
        ctx->UpdateSubresource(constants.Get(),0,nullptr,&settings,0,0); ctx->Dispatch(1,1,1);
        ctx->CopyResource(readback.Get(),output.Get()); D3D11_MAPPED_SUBRESOURCE mapped {};
        check(ctx->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
        std::array<Pixel,2> result; memcpy(result.data(),mapped.pData,sizeof(result)); ctx->Unmap(readback.Get(),0); return result;
    };
    auto result=run(); expect(same(result[0],edited[0]) && same(result[1],edited[1]), "Disabled filter changed output");
    settings.SkinProtection=1; result=run(); expect(same(result[0],edited[0]) && same(result[1],edited[1]), "Unity settings changed output");
    settings.SkinDetail=settings.SkinColour=settings.EnvironmentDetail=settings.EnvironmentColour=0;
    result=run(); expect(same(result[0],base[0]) && same(result[1],base[1]), "Zero strengths did not restore input");
    settings.EnvironmentDetail=settings.EnvironmentColour=1;
    result=run(); expect(same(result[0],base[0]) && same(result[1],edited[1]), "Skin/scene separation failed");
    settings.SkinDetail=1; result=run();
    expect(std::abs(result[0].r/result[0].g - base[0].r/base[0].g)<0.0001f &&
           std::abs(result[0].b/result[0].g - base[0].b/base[0].g)<0.0001f && same(result[1],edited[1]),
           "Skin colour suppression did not preserve chroma / affected environment");
    settings.ShowSkinMask=1; result=run(); expect(result[0].r>0.99f && result[1].r<0.01f, "Preview mismatch");
    settings.ShowSkinMask=0; settings.ApplyModel=0; result=run();
    expect(same(result[0],base[0]) && same(result[1],base[1]), "Model bypass did not preserve input");
    std::puts("PASS: disabled/unity identity, zero restore, skin/scene separation, preview, model bypass (WARP HLSL)");
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
