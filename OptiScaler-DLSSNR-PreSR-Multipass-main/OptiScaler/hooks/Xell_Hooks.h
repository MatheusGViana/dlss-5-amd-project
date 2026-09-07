#pragma once

#include <dxgi.h>
#include <d3d12.h>

#include <xell.h>
#include <xell_d3d12.h>

#include "Hook_Utils.h"

typedef decltype(&xellDestroyContext) PFN_xellDestroyContext;
typedef decltype(&xellSetSleepMode) PFN_xellSetSleepMode;
typedef decltype(&xellGetSleepMode) PFN_xellGetSleepMode;
typedef decltype(&xellSleep) PFN_xellSleep;
typedef decltype(&xellAddMarkerData) PFN_xellAddMarkerData;
typedef decltype(&xellGetVersion) PFN_xellGetVersion;
typedef decltype(&xellSetLoggingCallback) PFN_xellSetLoggingCallback;
typedef decltype(&xellGetFramesReports) PFN_xellGetFramesReports;
typedef decltype(&xellD3D12CreateContext) PFN_xellD3D12CreateContext;

class XellHooks
{
    inline static xell_context_handle_t ourContext = nullptr;
    inline static xell_context_handle_t gamesContext = nullptr;
    inline static bool gamesContextCanLimitFps = false;
    inline static bool blockExternal = false;

    inline static PFN_xellDestroyContext o_xellDestroyContext = nullptr;
    inline static PFN_xellSetSleepMode o_xellSetSleepMode = nullptr;
    inline static PFN_xellGetSleepMode o_xellGetSleepMode = nullptr;
    inline static PFN_xellSleep o_xellSleep = nullptr;
    inline static PFN_xellAddMarkerData o_xellAddMarkerData = nullptr;
    inline static PFN_xellGetVersion o_xellGetVersion = nullptr;
    inline static PFN_xellSetLoggingCallback o_xellSetLoggingCallback = nullptr;
    inline static PFN_xellD3D12CreateContext o_xellD3D12CreateContext = nullptr;

  public:
    static bool Hook();

    static void setOurContext(xell_context_handle_t context);
    static void blockExternalContexts(bool state);
    static bool shouldBlock(xell_context_handle_t context);
    static xell_context_handle_t getOurContext() { return ourContext; };

    static bool canLimit();
    static bool update();

    static xell_result_t hkxellDestroyContext(xell_context_handle_t context);
    static xell_result_t hkxellSetSleepMode(xell_context_handle_t context, const xell_sleep_params_t* param);
    static xell_result_t hkxellGetSleepMode(xell_context_handle_t context, xell_sleep_params_t* param);
    static xell_result_t hkxellSleep(xell_context_handle_t context, uint32_t frame_id);
    static xell_result_t hkxellAddMarkerData(xell_context_handle_t context, uint32_t frame_id,
                                             xell_latency_marker_type_t marker);
    static xell_result_t hkxellGetVersion(xell_version_t* pVersion);
    static xell_result_t hkxellSetLoggingCallback(xell_context_handle_t hContext, xell_logging_level_t loggingLevel,
                                                  xell_app_log_callback_t loggingCallback);
    static xell_result_t hkxellD3D12CreateContext(ID3D12Device* device, xell_context_handle_t* out_context);

    VALIDATE_MEMBER_HOOK(hkxellDestroyContext, PFN_xellDestroyContext)
    VALIDATE_MEMBER_HOOK(hkxellSetSleepMode, PFN_xellSetSleepMode)
    VALIDATE_MEMBER_HOOK(hkxellSleep, PFN_xellSleep)
    VALIDATE_MEMBER_HOOK(hkxellAddMarkerData, PFN_xellAddMarkerData)
    VALIDATE_MEMBER_HOOK(hkxellGetVersion, PFN_xellGetVersion)
    VALIDATE_MEMBER_HOOK(hkxellSetLoggingCallback, PFN_xellSetLoggingCallback)
    VALIDATE_MEMBER_HOOK(hkxellD3D12CreateContext, PFN_xellD3D12CreateContext)
    VALIDATE_MEMBER_HOOK(hkxellGetSleepMode, PFN_xellGetSleepMode)
};
