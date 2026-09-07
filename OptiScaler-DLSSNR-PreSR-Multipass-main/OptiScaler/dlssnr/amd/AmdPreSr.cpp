#include "AmdPreSr.h"
#include "RuntimeHash.h"
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <fstream>
#include <mutex>
#include <vector>
#include <cstring>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
namespace AmdPreSr
{
namespace
{
template <class T> T& At(HMODULE h, size_t rva) { return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(h) + rva); }
void Check(HRESULT hr, const char* operation)
{
    if (FAILED(hr))
        throw std::runtime_error(std::string(operation) + " HRESULT=" + std::to_string(static_cast<unsigned>(hr)));
}
void Barrier(ID3D12GraphicsCommandList* c, ID3D12Resource* r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b)
{
    if (!r || a == b)
        return;
    D3D12_RESOURCE_BARRIER v {};
    v.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    v.Transition = { r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, a, b };
    c->ResourceBarrier(1, &v);
}
struct Packet
{
    ID3D12GraphicsCommandList* list;
    ID3D12Resource* colour;
    UINT colourState, pad14;
    ID3D12Resource* motion;
    UINT motionState, pad24;
    ID3D12Resource* depth;
    UINT depthState, pad34;
    ID3D12Resource* exposure;
    UINT exposureState;
    float scaleX, scaleY;
    UINT pad4c;
};
static_assert(sizeof(Packet) == 0x50 && offsetof(Packet, scaleX) == 0x44);
using InitFn = bool(__fastcall*)(void*, const std::string*);
using RecordFn = void(__fastcall*)(Packet*);
using NotifyFn = void(__fastcall*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using HipSetFn = int (*)(int);
constexpr char CopyShader[] = R"(
Texture2D<float4> src : register(t0);
RWTexture2D<float4> dst : register(u0);
cbuffer Extent : register(b0) { uint w; uint h; };
[numthreads(8,8,1)] void main(uint3 p:SV_DispatchThreadID) {
 if(p.x<w && p.y<h) dst[p.xy]=src.Load(int3(p.xy,0));
})";
constexpr char DepthShader[] = R"(
Texture2D<float> src : register(t0);
RWTexture2D<float> dst : register(u0);
cbuffer Extent : register(b0) { uint w; uint h; };
[numthreads(8,8,1)] void main(uint3 p:SV_DispatchThreadID) {
 if(p.x<w && p.y<h) dst[p.xy]=src.Load(int3(p.xy,0));
})";
DXGI_FORMAT DepthReadFormat(DXGI_FORMAT f)
{
    switch (f)
    {
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R16_FLOAT:
        return DXGI_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}
std::string Layout(ID3D12Resource* resource)
{
    auto d = resource->GetDesc();
    return std::to_string(d.Width) + "x" + std::to_string(d.Height) + " format=" + std::to_string(d.Format) +
           " flags=" + std::to_string(d.Flags) + " samples=" + std::to_string(d.SampleDesc.Count) +
           " array=" + std::to_string(d.DepthOrArraySize) + " dimension=" + std::to_string(d.Dimension);
}
bool HashMatches(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), {});
    if (data.size() != 7156224)
        return false;
    BCRYPT_ALG_HANDLE alg {};
    unsigned char digest[32] {};
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return false;
    auto result = BCryptHash(alg, nullptr, 0, data.data(), static_cast<ULONG>(data.size()), digest, 32);
    BCryptCloseAlgorithmProvider(alg, 0);
    return result >= 0 && std::memcmp(digest, AmdRuntimeSha256, 32) == 0;
}
DXGI_FORMAT ReadFormat(DXGI_FORMAT f)
{
    switch (f)
    {
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    default:
        return f;
    }
}
} // namespace
struct Backend::Impl
{
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12Resource> colour;
    ComPtr<ID3D12Resource> motionCrop, depthCrop;
    ComPtr<ID3D12DescriptorHeap> heap;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12PipelineState> depthPipeline;
    std::array<HMODULE, 3> runtime {};
    std::array<UINT, 3> jobs {};
    std::filesystem::path directory;
    std::string status = "AMD pre-SR: not initialized";
    std::atomic<ID3D12CommandList*> pending { nullptr };
    std::atomic<bool> failed { false };
    std::atomic<UINT64> completion { 0 };
    UINT64 frames = 0, serial = 0;
    UINT width = 0, height = 0, activePasses = 0, lastPasses = 0;
    HipSetFn hipSet = nullptr;
    int hipDevice = -1;
    std::mutex lock;
    void Log(const std::string& s)
    {
        status = s;
        std::ofstream out(directory / L"amd_presr.log", std::ios::app);
        out << GetTickCount64() << " " << s << '\n';
    }
    void InitHip()
    {
        if (hipSet)
            return;
        HMODULE hip = LoadLibraryExW(L"amdhip64_7.dll", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!hip)
            throw std::runtime_error("amdhip64_7.dll not found");
        auto count = reinterpret_cast<int (*)(int*)>(GetProcAddress(hip, "hipGetDeviceCount"));
        auto props = reinterpret_cast<int (*)(void*, int)>(GetProcAddress(hip, "hipGetDevicePropertiesR0600"));
        hipSet = reinterpret_cast<HipSetFn>(GetProcAddress(hip, "hipSetDevice"));
        if (!count || !props || !hipSet)
            throw std::runtime_error("HIP R0600 API unavailable");
        int n = 0;
        if (count(&n) != 0)
            throw std::runtime_error("HIP device enumeration failed");
        auto luid = device->GetAdapterLuid();
        for (int i = 0; i < n; ++i)
        {
            // R0600 prefix: name[256], uuid[16], luid[8]. Oversized aligned storage.
            alignas(16) std::array<unsigned char, 8192> p {};
            if (props(p.data(), i) == 0 && std::memcmp(p.data() + 272, &luid, 8) == 0)
            {
                hipDevice = i;
                Log("HIP adapter: " + std::string(reinterpret_cast<char*>(p.data())));
                break;
            }
        }
        if (hipDevice < 0 || hipSet(hipDevice) != 0)
            throw std::runtime_error("No HIP adapter matches D3D12 LUID");
    }
    void InitPass(UINT i)
    {
        if (runtime[i])
            return;
        InitHip();
        auto path = directory / (L"dlssnr_amd_pass" + std::to_wstring(i + 1) + L".dll");
        if (!HashMatches(path))
            throw std::runtime_error("Private AMD runtime hash mismatch: pass " + std::to_string(i + 1));
        auto weights = directory / L"dlssnr_on_amd_weights.bin";
        if (!std::filesystem::exists(weights))
            throw std::runtime_error("dlssnr_on_amd_weights.bin is required");
        HMODULE h =
            LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!h)
            throw std::runtime_error("Private AMD runtime LoadLibrary failed: " + std::to_string(GetLastError()));
        HMODULE pinned {};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<LPCWSTR>(h), &pinned);
        // Retain module even on failure: CRT registered HIP kernels; no unsafe unloading.
        runtime[i] = h;
        At<ID3D12Device*>(h, 0x764c8) = device.Get();
        device->AddRef();
        At<ID3D12CommandQueue*>(h, 0x764d0) = queue.Get();
        queue->AddRef();
        At<int>(h, 0x76f20) = hipDevice;
        At<uint8_t>(h, 0x76be0) = 1; // configured inline; 0x76be1 is staging state
        At<uint8_t>(h, 0x76c8c) = 1; // external-memory interop
        At<uint8_t>(h, 0x76e1c) = 1; // enabled
        At<uint8_t>(h, 0x76e1e) = 1; // FSR inputs, no swapchain fallback
        At<uint8_t>(h, 0x76e1f) = 1; // depth
        At<int>(h, 0x76e20) = -1;    // auto tonemap by input format
        std::string file = weights.string();
        if (hipSet(hipDevice) != 0 || !reinterpret_cast<InitFn>(reinterpret_cast<uintptr_t>(h) + 0x12380)(
                                          reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(h) + 0x764d8), &file))
            throw std::runtime_error("AMD engine initialization failed");
        At<uint8_t>(h, 0x767f8) = 1;
        Log("Initialized independent AMD pass " + std::to_string(i + 1));
    }
    void InitShader()
    {
        if (root)
            return;
        D3D12_DESCRIPTOR_RANGE ranges[2] {};
        ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0 };
        ranges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1 };
        D3D12_ROOT_PARAMETER params[2] {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].DescriptorTable = { 2, ranges };
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[1].Constants = { 0, 0, 2 };
        D3D12_ROOT_SIGNATURE_DESC desc { 2, params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
        ComPtr<ID3DBlob> blob, error;
        Check(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error),
              "Root signature serialize");
        Check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)),
              "Root signature create");
        Check(D3DCompile(CopyShader, sizeof(CopyShader), "AMD active crop", nullptr, nullptr, "main", "cs_5_0",
                         D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &error),
              "Crop shader compile");
        D3D12_COMPUTE_PIPELINE_STATE_DESC ps {};
        ps.pRootSignature = root.Get();
        ps.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };
        Check(device->CreateComputePipelineState(&ps, IID_PPV_ARGS(&pipeline)), "Crop pipeline");
        Check(D3DCompile(DepthShader, sizeof(DepthShader), "AMD depth conversion", nullptr, nullptr, "main", "cs_5_0",
                         D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &error),
              "Depth shader compile");
        ps.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };
        Check(device->CreateComputePipelineState(&ps, IID_PPV_ARGS(&depthPipeline)), "Depth pipeline");
        D3D12_DESCRIPTOR_HEAP_DESC hd { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4,
                                        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
        Check(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)), "Crop heap");
    }
};
Backend::Backend(ID3D12Device* d, ID3D12CommandQueue* q, const std::filesystem::path& dir) : p(new Impl)
{
    p->device = d;
    p->queue = q;
    p->directory = dir;
    try
    {
        Check(d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&p->fence)), "Completion fence");
    }
    catch (const std::exception& e)
    {
        p->failed = true;
        p->Log(e.what());
    }
}
ID3D12Resource* Backend::Record(ID3D12GraphicsCommandList* cmd, const Frame& f, const Settings& cfg)
{
    std::lock_guard guard(p->lock);
    if (p->failed || !cmd || !f.colour || !f.motion || !f.depth)
        return nullptr;
    if (p->pending.load() || p->fence->GetCompletedValue() < p->completion.load())
        return nullptr;
    try
    {
        auto desc = f.colour->GetDesc();
        UINT w = f.width ? f.width : static_cast<UINT>(desc.Width), h = f.height ? f.height : desc.Height;
        if (p->frames == 0 || p->width != w || p->height != h)
        {
            p->Log("Input active=" + std::to_string(w) + "x" + std::to_string(h) + " colour=" + Layout(f.colour));
            p->Log("Input motion=" + Layout(f.motion) + " depth=" + Layout(f.depth));
        }
        if (!w || !h || w > desc.Width || h > desc.Height || desc.SampleDesc.Count != 1 || desc.DepthOrArraySize != 1 ||
            desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            throw std::runtime_error("Unsupported active colour extent/layout");
        // Crop padded render-resolution guides; display-resolution guides are rejected
        // by the NGX adapter, because cropping them would change motion coordinates.
        for (auto guide : { f.motion, f.depth })
        {
            auto gd = guide->GetDesc();
            if (gd.Width < w || gd.Height < h || gd.SampleDesc.Count != 1 || gd.DepthOrArraySize != 1 ||
                gd.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
                throw std::runtime_error(std::string("Unsupported AMD pre-SR ") +
                                         (guide == f.motion ? "motion: " : "depth: ") + Layout(guide));
        }
        const auto depthDesc = f.depth->GetDesc();
        const bool convertDepth =
            depthDesc.Format != DXGI_FORMAT_R32_FLOAT || (depthDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        if (convertDepth && (DepthReadFormat(depthDesc.Format) == DXGI_FORMAT_UNKNOWN ||
                             (depthDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)))
            throw std::runtime_error("Unsupported depth shader view: " + Layout(f.depth));
        if (f.motion->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
            throw std::runtime_error("Unsupported depth-stencil motion buffer: " + Layout(f.motion));
        p->activePasses = std::clamp(cfg.passes, 1u, 3u);
        bool passChange = p->lastPasses != p->activePasses;
        p->lastPasses = p->activePasses;
        for (UINT i = 0; i < p->activePasses; ++i)
            p->InitPass(i);
        p->InitShader();
        bool resize = p->width != w || p->height != h;
        if (resize)
        {
            p->colour.Reset();
            D3D12_HEAP_PROPERTIES hp {};
            hp.Type = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_DESC rd {};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            rd.Width = w;
            rd.Height = h;
            rd.DepthOrArraySize = 1;
            rd.MipLevels = 1;
            rd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            rd.SampleDesc.Count = 1;
            rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            Check(p->device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                     D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
                                                     IID_PPV_ARGS(&p->colour)),
                  "Active FP16 texture");
            p->width = w;
            p->height = h;
        }
        auto prepareGuide = [&](ID3D12Resource* source, ComPtr<ID3D12Resource>& crop)
        {
            auto rd = source->GetDesc();
            if (rd.Width == w && rd.Height == h)
                return source;
            if (!crop || crop->GetDesc().Width != w || crop->GetDesc().Height != h ||
                crop->GetDesc().Format != rd.Format)
            {
                crop.Reset();
                rd.Width = w;
                rd.Height = h;
                rd.MipLevels = 1;
                rd.Flags = D3D12_RESOURCE_FLAG_NONE;
                D3D12_HEAP_PROPERTIES hp {};
                hp.Type = D3D12_HEAP_TYPE_DEFAULT;
                Check(p->device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
                                                         IID_PPV_ARGS(&crop)),
                      "Guide crop");
            }
            return crop.Get();
        };
        auto motion = prepareGuide(f.motion, p->motionCrop);
        ID3D12Resource* depth = nullptr;
        if (convertDepth)
        {
            if (!p->depthCrop || p->depthCrop->GetDesc().Width != w || p->depthCrop->GetDesc().Height != h ||
                p->depthCrop->GetDesc().Format != DXGI_FORMAT_R32_FLOAT ||
                !(p->depthCrop->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
            {
                p->depthCrop.Reset();
                auto rd = p->colour->GetDesc();
                rd.Format = DXGI_FORMAT_R32_FLOAT;
                D3D12_HEAP_PROPERTIES hp {};
                hp.Type = D3D12_HEAP_TYPE_DEFAULT;
                Check(p->device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
                                                         IID_PPV_ARGS(&p->depthCrop)),
                      "Depth R32 texture");
            }
            depth = p->depthCrop.Get();
        }
        else
            depth = prepareGuide(f.depth, p->depthCrop);
        auto cpu = p->heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_SHADER_RESOURCE_VIEW_DESC srv {};
        srv.Format = ReadFormat(desc.Format);
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        p->device->CreateShaderResourceView(f.colour, &srv, cpu);
        cpu.ptr += p->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav {};
        uav.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        p->device->CreateUnorderedAccessView(p->colour.Get(), nullptr, &uav, cpu);
        if (convertDepth)
        {
            // Distinct descriptor slots: overwriting the colour descriptors here
            // would change the earlier dispatch when the GPU consumes the list.
            cpu.ptr += p->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            srv.Format = DepthReadFormat(depthDesc.Format);
            p->device->CreateShaderResourceView(f.depth, &srv, cpu);
            cpu.ptr += p->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            uav.Format = DXGI_FORMAT_R32_FLOAT;
            p->device->CreateUnorderedAccessView(depth, nullptr, &uav, cpu);
        }
        Barrier(cmd, f.colour, f.colourState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Barrier(cmd, p->colour.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmd->SetComputeRootSignature(p->root.Get());
        cmd->SetPipelineState(p->pipeline.Get());
        auto heap = p->heap.Get();
        cmd->SetDescriptorHeaps(1, &heap);
        cmd->SetComputeRootDescriptorTable(0, heap->GetGPUDescriptorHandleForHeapStart());
        UINT dims[] { w, h };
        cmd->SetComputeRoot32BitConstants(1, 2, dims, 0);
        cmd->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
        Barrier(cmd, p->colour.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Barrier(cmd, f.colour, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, f.colourState);
        Barrier(cmd, f.motion, f.motionState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Barrier(cmd, f.depth, f.depthState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Barrier(cmd, f.exposure, f.exposureState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        auto copyGuide = [&](ID3D12Resource* source, ID3D12Resource* dest)
        {
            if (source == dest)
                return;
            Barrier(cmd, source, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
            Barrier(cmd, dest, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION from {}, to {};
            from.pResource = source;
            to.pResource = dest;
            D3D12_BOX box { 0, 0, 0, w, h, 1 };
            cmd->CopyTextureRegion(&to, 0, 0, 0, &from, &box);
            Barrier(cmd, dest, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            Barrier(cmd, source, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        };
        copyGuide(f.motion, motion);
        if (convertDepth)
        {
            Barrier(cmd, depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            cmd->SetPipelineState(p->depthPipeline.Get());
            auto table = p->heap->GetGPUDescriptorHandleForHeapStart();
            table.ptr += 2 * p->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            cmd->SetComputeRootDescriptorTable(0, table);
            cmd->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
            Barrier(cmd, depth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
        else
            copyGuide(f.depth, depth);
        UINT accepted = 0;
        for (UINT i = 0; i < p->activePasses; ++i)
        {
            auto r = p->runtime[i];
            At<uint8_t>(r, 0x76e1d) = 1;
            // Engine +0x120 is the history-valid flag, +0x118 is the current
            // borrowed history view. Clear only at a quiescent frame boundary.
            if (f.reset || resize || passChange)
            {
                At<uint8_t>(r, 0x765f8) = 0;
                At<void*>(r, 0x765f0) = nullptr;
            }
            At<UINT>(r, 0x76e10) = f.depthInverted;
            At<uint8_t>(r, 0x76e14) = 1; // explicit depth convention, no heuristic
            At<float>(r, 0x76e30) = i == 0 ? cfg.tone : 0;
            At<float>(r, 0x76e34) = cfg.structure;
            At<float>(r, 0x76e38) = cfg.skin;
            Packet packet {};
            packet.list = cmd;
            packet.colour = p->colour.Get();
            packet.colourState = 4;
            packet.motion = motion;
            packet.motionState = 4;
            packet.depth = depth;
            packet.depthState = 4;
            packet.exposure = f.exposure;
            packet.exposureState = 4;
            packet.scaleX = f.motionScaleX;
            packet.scaleY = f.motionScaleY;
            UINT before = At<UINT>(r, 0x76d74);
            reinterpret_cast<RecordFn>(reinterpret_cast<uintptr_t>(r) + 0xa0b0)(&packet);
            p->jobs[i] = At<UINT>(r, 0x76d74);
            if (At<uint8_t>(r, 0x767fa) || p->jobs[i] == before)
            {
                p->failed = true;
                p->Log("AMD pass rejected frame: " + std::to_string(i + 1));
                break;
            }
            ++accepted;
        }
        Barrier(cmd, f.motion, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, f.motionState);
        Barrier(cmd, f.depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, f.depthState);
        Barrier(cmd, f.exposure, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, f.exposureState);
        p->activePasses = accepted;
        if (accepted)
        {
            ++p->frames;
            p->pending.store(cmd, std::memory_order_release);
        }
        if (p->failed)
            return nullptr;
        if (p->frames <= 3 || resize)
            p->Log("Recorded pre-SR " + std::to_string(w) + "x" + std::to_string(h) +
                   " passes=" + std::to_string(p->activePasses));
        return p->colour.Get();
    }
    catch (const std::exception& e)
    {
        p->failed = true;
        p->Log(e.what());
        return nullptr;
    }
}
void Backend::Submitted(ID3D12CommandQueue* queue, UINT n, ID3D12CommandList* const* lists)
{
    auto pending = p->pending.load(std::memory_order_acquire);
    if (!pending || queue != p->queue.Get())
        return;
    bool found = false;
    for (UINT i = 0; i < n; ++i)
        found |= lists[i] == pending;
    if (!found)
        return;
    std::lock_guard guard(p->lock);
    if (p->pending.load() != pending)
        return;
    for (UINT i = 0; i < p->activePasses; ++i)
    {
        auto h = p->runtime[i];
        reinterpret_cast<NotifyFn>(reinterpret_cast<uintptr_t>(h) + 0x4640)(queue, n, lists);
        // All runtimes use HIP stream 0. Publish the next pass only once the previous
        // worker finished; otherwise its capture-wait kernel could block the first pass.
        auto start = GetTickCount64();
        while (static_cast<UINT>(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&At<UINT>(h, 0x76c14)), 0,
                                                            0)) < p->jobs[i])
        {
            if (GetTickCount64() - start > 5000)
            {
                p->failed = true;
                p->Log("HIP completion timeout pass " + std::to_string(i + 1));
                break;
            }
            Sleep(1);
        }
    }
    UINT64 value = ++p->serial;
    if (FAILED(queue->Signal(p->fence.Get(), value)))
    {
        p->failed = true;
        p->Log("D3D12 completion Signal failed");
    }
    p->completion.store(value);
    p->pending.store(nullptr, std::memory_order_release);
    if (!p->failed && p->frames <= 3)
        p->Log("Completed AMD pre-SR passes=" + std::to_string(p->activePasses) + " at " + std::to_string(p->width) +
               "x" + std::to_string(p->height));
}
std::string Backend::Status() const
{
    std::lock_guard guard(p->lock);
    return p->status;
}
UINT64 Backend::RecordedFrames() const { return p->frames; }
bool Backend::Shutdown()
{
    std::lock_guard guard(p->lock);
    if (p->pending.load() || p->fence->GetCompletedValue() < p->completion.load())
        return false;
    for (auto h : p->runtime)
        if (h)
        {
            if (p->hipSet)
                p->hipSet(p->hipDevice);
            reinterpret_cast<void (*)()>(reinterpret_cast<uintptr_t>(h) + 0xc520)();
        }
    p->failed = true;
    p->Log("Workers stopped outside loader lock");
    return true;
}
} // namespace AmdPreSr
