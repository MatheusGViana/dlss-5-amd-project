#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "RtgiNative.h"
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <array>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
namespace AmdPreSr
{
namespace
{
void Require(HRESULT hr, const char* operation)
{
    if (FAILED(hr))
        throw std::runtime_error(std::string("RTGI: ") + operation + " HRESULT=" + std::to_string(UINT(hr)));
}
std::vector<char> Read(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("RTGI: missing asset " + path.filename().string());
    auto size = std::filesystem::file_size(path);
    if (!size || size > 32 * 1024 * 1024)
        throw std::runtime_error("RTGI: invalid asset size");
    std::vector<char> bytes(size);
    if (!stream.read(bytes.data(), bytes.size()))
        throw std::runtime_error("RTGI: truncated asset");
    return bytes;
}
} // namespace
struct RtgiNative::Impl
{
    struct Texture
    {
        ComPtr<ID3D12Resource> resource;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    };
    struct View
    {
        Texture* texture = nullptr;
        UINT mip = 0;
    };
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12DescriptorHeap> heap;
    std::map<std::string, ComPtr<ID3D12PipelineState>> pipelines;
    Texture trace, output;
    bool validHistory = false;
    UINT width = 0, height = 0, table = 0, stride;
    RtgiSettings lastSettings;
    ID3D12GraphicsCommandList* cmd = nullptr;
    static constexpr UINT TableSize = 18, TableCount = 64;
    Texture Make(UINT w, UINT h, DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT, UINT mips = 1, UINT depth = 1)
    {
        D3D12_RESOURCE_DESC desc {};
        desc.Dimension = depth > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = w;
        desc.Height = h;
        desc.DepthOrArraySize = depth;
        desc.MipLevels = mips;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        D3D12_HEAP_PROPERTIES hp {};
        hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        Texture result;
        Require(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &desc, result.state, nullptr,
                                                IID_PPV_ARGS(&result.resource)),
                "texture allocation");
        return result;
    }
    void State(Texture& texture, D3D12_RESOURCE_STATES target)
    {
        if (texture.state == target)
            return;
        D3D12_RESOURCE_BARRIER b {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition = { texture.resource.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, texture.state, target };
        cmd->ResourceBarrier(1, &b);
        texture.state = target;
    }
    void Run(const char* name, UINT w, UINT h, UINT block, const void* constants, UINT count,
             std::initializer_list<std::pair<UINT, View>> inputs, std::initializer_list<std::pair<UINT, View>> outputs)
    {
        if (table >= TableCount)
            throw std::runtime_error("RTGI: descriptor table capacity exceeded");
        std::array<View, 13> in {};
        std::array<View, 5> out {};
        for (auto [slot, view] : inputs)
        {
            in.at(slot) = view;
            State(*view.texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
        for (auto [slot, view] : outputs)
        {
            out.at(slot) = view;
            State(*view.texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        auto cpu = heap->GetCPUDescriptorHandleForHeapStart();
        cpu.ptr += SIZE_T(table) * TableSize * stride;
        for (auto view : in)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC d {};
            d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            d.Texture2D.MipLevels = 1;
            if (view.texture)
            {
                auto desc = view.texture->resource->GetDesc();
                d.Format = desc.Format;
                if (d.Format == DXGI_FORMAT_R32G8X24_TYPELESS)
                    d.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                if (d.Format == DXGI_FORMAT_R32_TYPELESS)
                    d.Format = DXGI_FORMAT_R32_FLOAT;
                if (d.Format == DXGI_FORMAT_R16_TYPELESS)
                    d.Format = DXGI_FORMAT_R16_UNORM;
                if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
                {
                    d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                    d.Texture3D.MipLevels = desc.MipLevels;
                }
                else
                    d.Texture2D.MipLevels = desc.MipLevels;
            }
            device->CreateShaderResourceView(view.texture ? view.texture->resource.Get() : nullptr, &d, cpu);
            cpu.ptr += stride;
        }
        for (auto view : out)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC d {};
            d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            d.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            if (view.texture)
            {
                auto desc = view.texture->resource->GetDesc();
                d.Format = desc.Format;
                d.Texture2D.MipSlice = view.mip;
                if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
                {
                    d.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                    d.Texture3D.WSize = desc.DepthOrArraySize;
                }
            }
            device->CreateUnorderedAccessView(view.texture ? view.texture->resource.Get() : nullptr, nullptr, &d, cpu);
            cpu.ptr += stride;
        }
        auto hp = heap.Get();
        cmd->SetDescriptorHeaps(1, &hp);
        cmd->SetComputeRootSignature(root.Get());
        cmd->SetPipelineState(pipelines.at(name).Get());
        auto gpu = heap->GetGPUDescriptorHandleForHeapStart();
        gpu.ptr += UINT64(table++) * TableSize * stride;
        cmd->SetComputeRootDescriptorTable(0, gpu);
        cmd->SetComputeRoot32BitConstants(1, count, constants, 0);
        cmd->Dispatch((w + block - 1) / block, (h + block - 1) / block, 1);
        for (auto [slot, view] : outputs)
        {
            D3D12_RESOURCE_BARRIER b {};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            b.UAV.pResource = view.texture->resource.Get();
            cmd->ResourceBarrier(1, &b);
        }
    }
    void Resize(UINT w, UINT h)
    {
        trace = Make(w, h);
        output = Make(w, h);
        width = w;
        height = h;
        validHistory = false;
    }
    void Clear(Texture& texture)
    {
        State(texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        D3D12_DESCRIPTOR_HEAP_DESC hd { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
        ComPtr<ID3D12DescriptorHeap> cpuHeap;
        Require(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&cpuHeap)), "clear descriptor");
        D3D12_UNORDERED_ACCESS_VIEW_DESC d {};
        d.Format = texture.resource->GetDesc().Format;
        d.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        auto cpu = heap->GetCPUDescriptorHandleForHeapStart();
        cpu.ptr += SIZE_T(table) * TableSize * stride;
        auto gpu = heap->GetGPUDescriptorHandleForHeapStart();
        gpu.ptr += UINT64(table++) * TableSize * stride;
        device->CreateUnorderedAccessView(texture.resource.Get(), nullptr, &d, cpu);
        device->CreateUnorderedAccessView(texture.resource.Get(), nullptr, &d,
                                          cpuHeap->GetCPUDescriptorHandleForHeapStart());
        auto hp = heap.Get();
        cmd->SetDescriptorHeaps(1, &hp);
        float zero[4] {};
        cmd->ClearUnorderedAccessViewFloat(gpu, cpuHeap->GetCPUDescriptorHandleForHeapStart(), texture.resource.Get(),
                                           zero, 0, nullptr);
        D3D12_RESOURCE_BARRIER b {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        b.UAV.pResource = texture.resource.Get();
        cmd->ResourceBarrier(1, &b);
    }
};
RtgiNative::RtgiNative(ID3D12Device* device, const std::filesystem::path& folder) : p(std::make_unique<Impl>())
{
    p->device = device;
    p->stride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_DESCRIPTOR_RANGE ranges[2] { { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 13, 0, 0, 0 },
                                       { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0, 0, 13 } };
    D3D12_ROOT_PARAMETER rp[2] {};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[0].DescriptorTable = { 2, ranges };
    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[1].Constants = { 0, 0, 20 };
    D3D12_STATIC_SAMPLER_DESC sampler {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    D3D12_STATIC_SAMPLER_DESC samplers[2] { sampler, sampler };
    samplers[1].ShaderRegister = 1;
    samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    D3D12_ROOT_SIGNATURE_DESC desc { 2, rp, 2, samplers, D3D12_ROOT_SIGNATURE_FLAG_NONE };
    ComPtr<ID3DBlob> blob, error;
    Require(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), "root signature");
    Require(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&p->root)),
            "root signature creation");
    D3D12_DESCRIPTOR_HEAP_DESC hd { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Impl::TableCount * Impl::TableSize,
                                    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
    Require(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&p->heap)), "descriptor heap");
    for (auto name : { "GatherCS", "ResolveCS" })
    {
        auto bytes = Read(folder / (std::string(name) + ".cso"));
        D3D12_COMPUTE_PIPELINE_STATE_DESC ps {};
        ps.pRootSignature = p->root.Get();
        ps.CS = { bytes.data(), bytes.size() };
        Require(device->CreateComputePipelineState(&ps, IID_PPV_ARGS(&p->pipelines[name])), name);
    }
}

RtgiNative::~RtgiNative() = default;
void RtgiNative::ResetHistory() { p->validHistory = false; }
ID3D12Resource* RtgiNative::Record(ID3D12GraphicsCommandList* cmd, const Frame& frame, const RtgiSettings& requested)
{
    auto cfg = requested;
    auto bound = [](float value, float lo, float hi, float fallback)
    { return std::isfinite(value) ? std::clamp(value, lo, hi) : fallback; };
    cfg.contact = bound(cfg.contact, 0, 2, 0);
    cfg.saturation = bound(cfg.saturation, 0, 2, 1);
    cfg.radius = bound(cfg.radius, .25f, 3, 1);
    cfg.mix = bound(cfg.mix, 0, 1, 1);
    cfg.lighting = bound(cfg.lighting, 0, 10, 5);
    cfg.occlusion = bound(cfg.occlusion, 0, 10, 1);
    cfg.ambient = bound(cfg.ambient, .25f, 1, 1);
    cfg.thickness = bound(cfg.thickness, 0, 1, .1f);
    cfg.smoothness = bound(cfg.smoothness, 0, 1, .5f);
    cfg.fade = bound(cfg.fade, .001f, 1, .3f);
    cfg.fov = bound(cfg.fov, 20, 140, 60);
    cfg.farPlane = bound(cfg.farPlane, 10, 100000, 600);
    cfg.quality = std::min(cfg.quality, 4u);
    cfg.denoiser = std::min(cfg.denoiser, 2u);
    cfg.inspect = std::min(cfg.inspect, 1u);
    if (!cfg.enabled || cfg.mix <= 0)
    {
        p->validHistory = false;
        return frame.colour;
    }
    if (frame.width < 32 || frame.height < 32 || !frame.colour || !frame.depth || !frame.motion)
        throw std::runtime_error("RTGI: invalid input");
    auto format = frame.depth->GetDesc().Format;
    if (format != DXGI_FORMAT_R32_FLOAT && format != DXGI_FORMAT_R32_TYPELESS && format != DXGI_FORMAT_R16_FLOAT &&
        format != DXGI_FORMAT_R16_UNORM && format != DXGI_FORMAT_R16_TYPELESS &&
        format != DXGI_FORMAT_R32G8X24_TYPELESS)
        throw std::runtime_error("RTGI: depth conversion required for this format");
    if (p->width != frame.width || p->height != frame.height)
        p->Resize(frame.width, frame.height);
    if (frame.reset || !(cfg == p->lastSettings))
        p->validHistory = false;
    p->lastSettings = cfg;
    p->cmd = cmd;
    p->table = 0;
    Impl::Texture colour { frame.colour, frame.colourState }, depth { frame.depth, frame.depthState };
    struct Constants {
        UINT w, h, reverse, quality;
        float farPlane, tanHalf, thickness, fade;
        float lighting, occlusion, ambient, mix;
        UINT inspect, denoiser;
        float smoothness, pad;
        float contact, saturation, radius, pad2;
    } c { frame.width, frame.height, UINT(frame.depthInverted), cfg.quality,
          cfg.farPlane, std::tan(cfg.fov * .00872664626f), cfg.thickness, cfg.fade,
          cfg.lighting, cfg.occlusion, cfg.ambient, cfg.mix, cfg.inspect, cfg.denoiser, cfg.smoothness, 0, cfg.contact, cfg.saturation, cfg.radius, 0 };
    p->Run("GatherCS", c.w, c.h, 8, &c, 20,
           { { 0, { &colour } }, { 1, { &depth } } }, { { 0, { &p->trace } } });
    p->Run("ResolveCS", c.w, c.h, 8, &c, 20,
           { { 0, { &colour } }, { 1, { &depth } }, { 2, { &p->trace } } }, { { 0, { &p->output } } });
    p->State(p->output, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    p->State(colour, frame.colourState);
    p->State(depth, frame.depthState);
    return p->output.resource.Get();
}
} // namespace AmdPreSr
