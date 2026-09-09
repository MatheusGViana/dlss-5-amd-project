#include "pch.h"
#include "AmdBridge.h"
#include "AmdPreSr.h"
#include "PresentExperimental.h"
#include <State.h>
#include <Util.h>
#include <misc/SkipSpoof.h>
#include <detours/detours.h>
#include <atomic>
#include <fstream>
#include <mutex>
#include <unordered_set>

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
std::mutex observedMutex;
std::unordered_set<ID3D12CommandList*> observedLists;
void Message(const char* s)
{
    std::lock_guard l(messageMutex);
    if (*s && message != s)
    {
        std::ofstream log(Util::DllPath().parent_path() / L"amd_bridge.log", std::ios::app);
        log << GetTickCount64() << " thread=" << GetCurrentThreadId() << " " << s << '\n';
    }
    message = s;
}
thread_local NVSDK_NGX_Parameter* replacedParams = nullptr;
thread_local ID3D12Resource* originalColour = nullptr;
struct FrameIdentity
{
    ID3D12Resource* colour = nullptr;
    ID3D12Resource* motion = nullptr;
    ID3D12Resource* depth = nullptr;
    UINT width = 0, height = 0;
};
std::mutex frameMutex;
FrameIdentity lastFrame {};
UINT stableFrames = 0;
void ExecuteBatch(ID3D12CommandQueue* q, UINT n, ID3D12CommandList* const* c)
{
    if (auto b = backend.load())
        b->Submitting(q, n, c);
    executeOriginal(q, n, c);
    {
        std::lock_guard guard(observedMutex);
        if (observedLists.size() > 256)
            observedLists.clear();
        for (UINT i = 0; i < n; ++i)
            observedLists.insert(c[i]);
    }
    if (auto b = backend.load())
        b->Submitted(q, n, c);
}
void STDMETHODCALLTYPE Execute(ID3D12CommandQueue* q, UINT n, ID3D12CommandList* const* c)
{
    auto b = backend.load();
    int index = b ? b->PendingListIndex(n, c) : -1;
    if (n > 1 && index >= 0)
    {
        // Separate Execute calls establish an execution boundary around the
        // interop list. Preserve list order and execute each list exactly once.
        Message("AMD isolated neural command list from a render batch");
        if (index) ExecuteBatch(q, static_cast<UINT>(index), c);
        ExecuteBatch(q, 1, c + index);
        auto remaining = n - static_cast<UINT>(index) - 1;
        if (remaining) ExecuteBatch(q, remaining, c + index + 1);
        return;
    }
    ExecuteBatch(q, n, c);
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
    struct PhysicalAdapterScope
    {
        uint64_t id = SkipSpoof::AddEntry(SkipSpoofType::Thread);
        ~PhysicalAdapterScope() { SkipSpoof::RemoveEntry(id); }
    } physicalAdapterScope;
    IDXGIFactory4* f = nullptr;
    IDXGIAdapter1* a = nullptr;
    bool amd = false;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&f))))
    {
        if (SUCCEEDED(f->EnumAdapterByLuid(d->GetAdapterLuid(), IID_PPV_ARGS(&a))))
        {
            DXGI_ADAPTER_DESC1 desc {};
            amd = SUCCEEDED(a->GetDesc1(&desc)) && desc.VendorId == 0x1002;
            LOG_INFO("AMD pre-SR physical adapter vendor: {:04X}, AMD: {}", desc.VendorId, amd);
            a->Release();
        }
        f->Release();
    }
    return amd;
}
} // namespace
bool HasFiles()
{
    // Proxy names such as winmm.dll can load before Util::DllPath is finalized.
    // A negative result cached at that point disabled the AMD backend for the
    // rest of the process and left the menu at "waiting for a DirectX 12 SR
    // frame". Recheck until the package path becomes available.
    std::error_code ec;
    return std::filesystem::exists(Directory() / L"dlssnr_amd_pass1.dll", ec);
}
bool Before(ID3D12GraphicsCommandList* cmd, NVSDK_NGX_Parameter* params, ID3D12CommandQueue* q)
{
    // A single backend consumes one SR stream even if the engine rotates worker threads.
    // Serialize shared settling/identity state; thread-local replacement ownership stays unchanged.
    std::lock_guard frameGuard(frameMutex);
    if (!HasFiles())
        return false;
    ID3D12Device* device = nullptr;
    if (!cmd || !params || FAILED(cmd->GetDevice(IID_PPV_ARGS(&device))))
        return true;
    thread_local LUID checkedAdapter {};
    thread_local bool checked = false, amd = false;
    const auto adapter = device->GetAdapterLuid();
    if (!checked || adapter.HighPart != checkedAdapter.HighPart || adapter.LowPart != checkedAdapter.LowPart)
    {
        amd = IsAmd(device);
        checkedAdapter = adapter;
        checked = true;
    }
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
        // FG can expose a proxy present queue. Hook the device's execution
        // implementation so actual render submissions are still observed.
        ID3D12CommandQueue* probe = nullptr;
        D3D12_COMMAND_QUEUE_DESC queueDesc {};
        if (SUCCEEDED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&probe))))
        {
            executeOriginal = reinterpret_cast<ExecuteFn>((*reinterpret_cast<void***>(probe))[10]);
            probe->Release();
        }
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
        backend.store(b);
    }
    device->Release();
    // The hook observes this list when the current frame is submitted and then
    // binds the actual queue before waking HIP. Engines that rotate command-list
    // objects may never submit the same object twice, so do not require a prior
    // observation here.
    Message("");
    // The swapchain's present queue can change when FG is enabled. It is
    // only a bootstrap hint; Submitted identifies the queue executing our list.
    AmdPreSr::Frame f {};
    f.colour = Resource(params, NVSDK_NGX_Parameter_Color);
    f.motion = Resource(params, NVSDK_NGX_Parameter_MotionVectors);
    f.depth = Resource(params, NVSDK_NGX_Parameter_Depth);
    f.exposure = Resource(params, NVSDK_NGX_Parameter_ExposureTexture);
    params->Get(NVSDK_NGX_Parameter_DLSS_Pre_Exposure, &f.preExposure);
    params->Get(NVSDK_NGX_Parameter_DLSS_Exposure_Scale, &f.exposureScale);
    params->Get(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, &f.width);
    params->Get(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, &f.height);
    if (f.colour)
    {
        const auto extent = f.colour->GetDesc();
        if (!f.width) f.width = static_cast<UINT>(extent.Width);
        if (!f.height) f.height = extent.Height;
    }
    UINT x = 0, y = 0, flags = 0, reset = 0;
    params->Get(NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X, &x);
    params->Get(NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y, &y);
    if (x || y)
    {
        Message("AMD pre-SR: nonzero colour subrect origin unsupported");
        return true;
    }
    auto haveFlags = params->Get(NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, &flags) == NVSDK_NGX_Result_Success;
    if (haveFlags && !(flags & NVSDK_NGX_DLSS_Feature_Flags_MVLowRes) && f.motion)
    {
        params->Get(NVSDK_NGX_Parameter_OutWidth, &f.motionWidth);
        params->Get(NVSDK_NGX_Parameter_OutHeight, &f.motionHeight);
        if (!f.motionWidth) f.motionWidth = static_cast<UINT>(f.motion->GetDesc().Width);
        if (!f.motionHeight) f.motionHeight = f.motion->GetDesc().Height;
    }
    // Let SR finish its reconfiguration before rebuilding the private HIP model.
    // Do not retain or replay the old image while input sizes are settling.
    static UINT settlingWidth=0, settlingHeight=0;
    static float settlingScale=1.f;
    static ULONGLONG settlingSince=0;
    const float sessionScale=Config::Instance()->AmdNrScale.value_or_default();
    const float requestedScale=sessionScale;
    const auto now=GetTickCount64();
    if(settlingWidth!=f.width || settlingHeight!=f.height || settlingScale!=requestedScale) {
        settlingWidth=f.width;settlingHeight=f.height;settlingScale=requestedScale;settlingSince=now;
        b->InvalidateHistory();
    }
    if(now-settlingSince<300) {
        Message("AMD neural: waiting for resolution settings to settle");
        return true;
    }
    const FrameIdentity current { f.colour, f.motion, f.depth, f.width, f.height };
    // Resource addresses rotate in Unreal's frame buffers. Only an extent
    // change requires warm-up; pointer equality can suppress every frame.
    const bool sameFrame = current.width == lastFrame.width &&
                           current.height == lastFrame.height;
    if (!sameFrame)
    {
        lastFrame = current;
        stableFrames = 0;
        b->InvalidateHistory();
        Message("AMD pre-SR: warming up after an upscaler/resource change");
        return true;
    }
    if (stableFrames < 2 && ++stableFrames < 2)
    {
        Message("AMD pre-SR: warming up after an upscaler/resource change");
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
    s.rtgi.enabled = cfg.AmdRtgiEnabled.value_or_default();
    s.rtgi.quality = cfg.AmdRtgiQuality.value_or_default();
    s.rtgi.denoiser = cfg.AmdRtgiDenoiser.value_or_default();
    s.rtgi.inspect = cfg.AmdRtgiInspect.value_or_default();
    s.rtgi.contact = cfg.AmdRtgiContact.value_or_default();
    s.rtgi.saturation = cfg.AmdRtgiSaturation.value_or_default();
    s.rtgi.radius = cfg.AmdRtgiRadius.value_or_default();
    s.rtgi.mix = cfg.AmdRtgiMix.value_or_default();
    s.rtgi.lighting = cfg.AmdRtgiLighting.value_or_default();
    s.rtgi.occlusion = cfg.AmdRtgiOcclusion.value_or_default();
    s.rtgi.ambient = cfg.AmdRtgiAmbient.value_or_default();
    s.rtgi.thickness = cfg.AmdRtgiThickness.value_or_default();
    s.rtgi.smoothness = cfg.AmdRtgiSmoothness.value_or_default();
    s.rtgi.fade = cfg.AmdRtgiFade.value_or_default();
    s.rtgi.fov = cfg.AmdRtgiFov.value_or_default();
    s.rtgi.farPlane = cfg.AmdRtgiFarPlane.value_or_default();
    s.look.enabled = cfg.AmdLookEnabled.value_or_default();
    s.look.appearance = 2; // Single default profile; ignore legacy preset selections.
    s.look.mix = cfg.AmdLookMix.value_or_default();
    s.look.materialDetail = cfg.AmdLookMaterialDetail.value_or_default();
    s.look.shapeDefinition = cfg.AmdLookShapeDefinition.value_or_default();
    s.look.localLighting = cfg.AmdLookLocalLighting.value_or_default();
    s.look.skinDetail = cfg.AmdLookSkinDetail.value_or_default();
    s.look.skinSoftness = cfg.AmdLookSkinSoftness.value_or_default();
    s.look.detectSkin = cfg.AmdLookDetectSkin.value_or_default();
    s.look.specularControl = cfg.AmdLookSpecularControl.value_or_default();
    s.look.highlightRollOff = cfg.AmdLookHighlightRollOff.value_or_default();
    s.look.colourSeparation = cfg.AmdLookColourSeparation.value_or_default();
    s.look.shadowDepth = cfg.AmdLookShadowDepth.value_or_default();
    s.look.antiHalo = cfg.AmdLookAntiHalo.value_or_default();
    s.look.flatAreaProtection = cfg.AmdLookFlatAreaProtection.value_or_default();
    s.look.inspect = cfg.AmdLookInspect.value_or_default();
    s.look.tone = cfg.AmdLookTone.value_or_default();
    s.look.exposureEV = cfg.AmdLookExposureEV.value_or_default();
    s.look.contrast = cfg.AmdLookContrast.value_or_default();
    s.look.saturation = cfg.AmdLookSaturation.value_or_default();
    s.look.highlightCompression = cfg.AmdLookHighlightCompression.value_or_default();
    s.modelScale = sessionScale;
    s.passes = cfg.DlssNrPasses.value_or_default();
    // The pinned AMD binary explicitly disables the broad lighting/colour
    // channels. Its embedded UI warns that nonzero tone mostly darkens frames.
    s.encoding=std::clamp(cfg.AmdEncoding.value_or_default(),0,3);
    s.toneChannels=cfg.AmdNeuralLightingStrength.value_or_default()>0;
    s.tone=s.toneChannels ? std::clamp(cfg.AmdNeuralLightingStrength.value_or_default(),0.f,1.f) : 0.f;
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
bool HasReplacement(NVSDK_NGX_Parameter* params)
{
    return params && replacedParams == params && originalColour;
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
void InvalidateHistory()
{
    if (auto b = backend.load())
        b->InvalidateHistory();
}
std::string Status()
{
    if(AmdPresentExperimental::IsTarget()) return AmdPresentExperimental::Status();
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
