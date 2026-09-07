#include "pch.h"
#include "AmdBridge.h"
#include "AmdPreSr.h"
#include <State.h>
#include <Util.h>
#include <detours/detours.h>
#include <atomic>
#include <mutex>

namespace DlssNr::AmdBridge
{
namespace
{
std::atomic<AmdPreSr::Backend*> backend { nullptr };
using ExecuteFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using ExitFn = void(NTAPI*)(LONG);
ExecuteFn executeOriginal = nullptr;
ExitFn exitOriginal = nullptr;
std::string message = "AMD pre-SR: waiting for a DirectX 12 SR frame";
std::mutex messageMutex;
std::mutex initMutex;
ID3D12CommandQueue* installedQueue = nullptr;
void Message(const char* s)
{
    std::lock_guard l(messageMutex);
    message = s;
}
thread_local NVSDK_NGX_Parameter* replacedParams = nullptr;
thread_local ID3D12Resource* originalColour = nullptr;
void STDMETHODCALLTYPE Execute(ID3D12CommandQueue* q, UINT n, ID3D12CommandList* const* c)
{
    executeOriginal(q, n, c);
    if (auto b = backend.load())
        b->Submitted(q, n, c);
}
void NTAPI Exit(LONG code)
{
    if (auto b = backend.load())
        b->Shutdown();
    exitOriginal(code);
}
std::filesystem::path Directory() { return Util::DllPath().parent_path(); }
ID3D12Resource* Resource(NVSDK_NGX_Parameter* p, const char* name)
{
    ID3D12Resource* r = nullptr;
    if (p->Get(name, &r) != NVSDK_NGX_Result_Success)
        p->Get(name, reinterpret_cast<void**>(&r));
    return r;
}
bool IsAmd(ID3D12Device* d)
{
    IDXGIFactory4* f = nullptr;
    IDXGIAdapter1* a = nullptr;
    bool amd = false;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&f))))
    {
        if (SUCCEEDED(f->EnumAdapterByLuid(d->GetAdapterLuid(), IID_PPV_ARGS(&a))))
        {
            DXGI_ADAPTER_DESC1 desc {};
            amd = SUCCEEDED(a->GetDesc1(&desc)) && desc.VendorId == 0x1002;
            a->Release();
        }
        f->Release();
    }
    return amd;
}
} // namespace
bool HasFiles()
{
    std::error_code ec;
    return std::filesystem::exists(Directory() / L"dlssnr_amd_pass1.dll", ec);
}
bool Before(ID3D12GraphicsCommandList* cmd, NVSDK_NGX_Parameter* params, ID3D12CommandQueue* q)
{
    if (!HasFiles())
        return false;
    ID3D12Device* device = nullptr;
    if (!cmd || !params || FAILED(cmd->GetDevice(IID_PPV_ARGS(&device))))
        return true;
    const bool amd = IsAmd(device);
    if (!amd)
    {
        device->Release();
        return false;
    }
    if (!q)
        q = reinterpret_cast<ID3D12CommandQueue*>(State::Instance().currentCommandQueue);
    if (!q)
    {
        device->Release();
        Message("AMD pre-SR: waiting for the game command queue");
        return true;
    }
    std::lock_guard initGuard(initMutex);
    auto b = backend.load();
    if (!b)
    {
        executeOriginal = reinterpret_cast<ExecuteFn>((*reinterpret_cast<void***>(q))[10]);
        exitOriginal = reinterpret_cast<ExitFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlExitUserProcess"));
        LONG err = DetourTransactionBegin();
        if (err == NO_ERROR)
            err = DetourUpdateThread(GetCurrentThread());
        if (err == NO_ERROR)
            err = DetourAttach(reinterpret_cast<PVOID*>(&executeOriginal), Execute);
        if (err == NO_ERROR && exitOriginal)
            err = DetourAttach(reinterpret_cast<PVOID*>(&exitOriginal), Exit);
        if (err == NO_ERROR)
            err = DetourTransactionCommit();
        else
            DetourTransactionAbort();
        if (err != NO_ERROR)
        {
            device->Release();
            Message("AMD pre-SR: could not install submission notification");
            return true;
        }
        b = new AmdPreSr::Backend(device, q, Directory());
        installedQueue = q;
        backend.store(b);
    }
    device->Release();
    Message("");
    if (q != installedQueue)
    {
        Message("AMD pre-SR: multiple command queues unsupported");
        return true;
    }
    AmdPreSr::Frame f {};
    f.colour = Resource(params, NVSDK_NGX_Parameter_Color);
    f.motion = Resource(params, NVSDK_NGX_Parameter_MotionVectors);
    f.depth = Resource(params, NVSDK_NGX_Parameter_Depth);
    f.exposure = Resource(params, NVSDK_NGX_Parameter_ExposureTexture);
    params->Get(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, &f.width);
    params->Get(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, &f.height);
    UINT x = 0, y = 0, flags = 0, reset = 0;
    params->Get(NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X, &x);
    params->Get(NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y, &y);
    if (x || y)
    {
        Message("AMD pre-SR: nonzero colour subrect origin unsupported");
        return true;
    }
    auto haveFlags = params->Get(NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, &flags) == NVSDK_NGX_Result_Success;
    if (haveFlags && !(flags & NVSDK_NGX_DLSS_Feature_Flags_MVLowRes) && f.motion &&
        (f.motion->GetDesc().Width != f.width || f.motion->GetDesc().Height != f.height))
    {
        Message("AMD pre-SR: display-resolution motion vectors unsupported");
        return true;
    }
    f.depthInverted = (flags & NVSDK_NGX_DLSS_Feature_Flags_DepthInverted) != 0;
    params->Get(NVSDK_NGX_Parameter_Reset, &reset);
    f.reset = reset != 0;
    params->Get(NVSDK_NGX_Parameter_MV_Scale_X, &f.motionScaleX);
    params->Get(NVSDK_NGX_Parameter_MV_Scale_Y, &f.motionScaleY);
    const auto& cfg = *Config::Instance();
    if (cfg.ColorResourceBarrier.has_value())
        f.colourState = static_cast<D3D12_RESOURCE_STATES>(cfg.ColorResourceBarrier.value());
    if (cfg.MVResourceBarrier.has_value())
        f.motionState = static_cast<D3D12_RESOURCE_STATES>(cfg.MVResourceBarrier.value());
    if (cfg.DepthResourceBarrier.has_value())
        f.depthState = static_cast<D3D12_RESOURCE_STATES>(cfg.DepthResourceBarrier.value());
    if (cfg.ExposureResourceBarrier.has_value())
        f.exposureState = static_cast<D3D12_RESOURCE_STATES>(cfg.ExposureResourceBarrier.value());
    AmdPreSr::Settings s {};
    s.passes = cfg.DlssNrPasses.value_or_default();
    s.tone = cfg.DlssNrLocalTone.value_or_default();
    s.structure = cfg.DlssNrLocalStructure.value_or_default();
    s.skin = cfg.DlssNrSkinStructure.value_or_default();
    if (s.skin < 0)
        s.skin = s.structure;
    if (auto replacement = b->Record(cmd, f, s))
    {
        originalColour = f.colour;
        replacedParams = params;
        params->Set(NVSDK_NGX_Parameter_Color, replacement);
    }
    return true;
}
void Restore(NVSDK_NGX_Parameter* params)
{
    if (params && replacedParams == params)
    {
        params->Set(NVSDK_NGX_Parameter_Color, originalColour);
        replacedParams = nullptr;
        originalColour = nullptr;
    }
}
std::string Status()
{
    {
        std::lock_guard l(messageMutex);
        if (!message.empty())
            return message;
    }
    if (auto b = backend.load())
        return b->Status();
    return "AMD pre-SR: idle";
}
} // namespace DlssNr::AmdBridge
