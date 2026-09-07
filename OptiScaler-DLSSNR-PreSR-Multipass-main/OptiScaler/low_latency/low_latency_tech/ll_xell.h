#pragma once

#include "low_latency_tech.h"

#include <xell_d3d12.h>
#include "low_latency/input/input_xell.h"
#include <proxies/XeLL_Proxy.h>

class XeLL : public LowLatencyTech
{
    bool sent_sleep_frame_ids[64] {};
    uint64_t simulation_start_last_id {};
    uint64_t sleep_last_id {};

    xell_context_handle_t xell_context {};

    decltype(&xellDestroyContext) o_xellDestroyContext = nullptr;
    decltype(&xellSetSleepMode) o_xellSetSleepMode = nullptr;
    decltype(&xellGetSleepMode) o_xellGetSleepMode = nullptr;
    decltype(&xellSleep) o_xellSleep = nullptr;
    decltype(&xellAddMarkerData) o_xellAddMarkerData = nullptr;
    decltype(&xellGetVersion) o_xellGetVersion = nullptr;
    decltype(&xellSetLoggingCallback) o_xellSetLoggingCallback = nullptr;
    decltype(&xellGetFramesReports) o_xellGetFramesReports = nullptr;
    decltype(&xellD3D12CreateContext) o_xellD3D12CreateContext = nullptr;

    decltype(&xellD3D12SetAppQueue) o_xellD3D12SetAppQueue = nullptr;
    decltype(&xellSetDisplayInfo) o_xellSetDisplayInfo = nullptr;
    decltype(&xellSetFgEnabled) o_xellSetFgEnabled = nullptr;
    decltype(&xellSetGeneratedFramesCount) o_xellSetGeneratedFramesCount = nullptr;
    decltype(&xellGetLastPresentStartFrameId) o_xellGetLastPresentStartFrameId = nullptr; // Maybe not needed

    void xell_sleep(uint32_t frame_id);
    void add_marker(uint32_t frame_id, xell_latency_marker_type_t marker);

  public:
    XeLL() : LowLatencyTech()
    {
        if (XeLLProxy::InitXeLL())
        {
#ifdef LOW_LATENCY_INPUTS
            o_xellDestroyContext = XeLLProxy::RealDestroyContext();
            o_xellSetSleepMode = XeLLProxy::RealSetSleepMode();
            o_xellGetSleepMode = XeLLProxy::RealGetSleepMode();
            o_xellSleep = XeLLProxy::RealSleep();
            o_xellAddMarkerData = XeLLProxy::RealAddMarkerData();
            o_xellGetVersion = XeLLProxy::RealGetVersion();
            o_xellSetLoggingCallback = XeLLProxy::RealSetLoggingCallback();
            o_xellGetFramesReports = XeLLProxy::RealGetFramesReports();
            o_xellD3D12CreateContext = XeLLProxy::RealD3D12CreateContext();

            o_xellD3D12SetAppQueue = XeLLProxy::RealD3D12SetAppQueue();
            o_xellSetDisplayInfo = XeLLProxy::RealSetDisplayInfo();
            o_xellSetFgEnabled = XeLLProxy::RealSetFgEnabled();
            o_xellSetGeneratedFramesCount = XeLLProxy::RealSetGeneratedFramesCount();
            o_xellGetLastPresentStartFrameId = XeLLProxy::RealGetLastPresentStartFrameId(); // Maybe not needed
#else
            o_xellDestroyContext = XeLLProxy::DestroyContext();
            o_xellSetSleepMode = XeLLProxy::SetSleepMode();
            o_xellGetSleepMode = XeLLProxy::GetSleepMode();
            o_xellSleep = XeLLProxy::Sleep();
            o_xellAddMarkerData = XeLLProxy::AddMarkerData();
            o_xellGetVersion = XeLLProxy::GetVersion();
            o_xellSetLoggingCallback = XeLLProxy::SetLoggingCallback();
            o_xellGetFramesReports = XeLLProxy::GetFramesReports();
            o_xellD3D12CreateContext = XeLLProxy::D3D12CreateContext();
#endif
        }
    }

    // From LowLatencyTech
    bool init(IUnknown* pDevice) override;
    void deinit() override;

    LowLatencyMode get_mode() override { return LowLatencyMode::XeLL; };
    void* get_tech_context() override;
    void set_fg_type(bool interpolated, uint64_t frame_id) override {}; // Not used by XeLL
    void set_low_latency_override(ForceReflex low_latency_override) override
    {
        this->low_latency_override = low_latency_override;
    };
    void set_effective_fg_state(bool effective_fg_state) override { this->effective_fg_state = effective_fg_state; };

    bool is_enabled() override
    {
        return low_latency_override != ForceReflex::InGame ? low_latency_override == ForceReflex::ForceEnable
                                                           : low_latency_enabled;
    };

    void get_sleep_status(SleepParams* sleep_params) override;
    void set_sleep_mode(SleepMode* sleep_mode) override;
    void sleep(std::optional<uint32_t> frame_id) override;
    void set_marker(IUnknown* pDevice, const MarkerParams& marker_params) override;
    void set_async_marker(IUnknown* pCommandQueue, const MarkerParams& marker_params) override;

    // For passthrough
    xell_result_t xellD3D12SetAppQueue(ID3D12CommandQueue* appQueue) const;
    xell_result_t xellSetDisplayInfo(void* displayInfo) const;
    xell_result_t xellSetFgEnabled(uint32_t param1, uint32_t param2) const;
    xell_result_t xellSetGeneratedFramesCount(uint32_t frameId, uint32_t framesCount) const;
    xell_result_t xellGetLastPresentStartFrameId(uint32_t* p_frame_id) const;
    xell_result_t xellGetFramesReports(xell_frame_report_t* outdata) const;
};