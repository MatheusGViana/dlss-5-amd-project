#pragma once

#include "input_common.h"

typedef void (*PFN_FLTickExternal)(int64_t frameId, float DeltaSeconds, bool bIdleMode);
typedef void (*PFN_setTickCallback)(PFN_FLTickExternal callback);

class InputUeLowLatency
{
    inline static bool inited = false;
    inline static IUnknown* device = nullptr;

    inline static InputContext inputContext { .caller = LowLatencyInput::UeLowLatency,
                                              .localContext = false,
                                              .noFrameId = false,
                                              .markerMode = InputMarkerMode::SimOnly };

  public:
    static void init();
    static void tickEnd(int64_t frameId, float DeltaSeconds, bool bIdleMode);
    static void tickStart(int64_t frameId, float DeltaSeconds, bool bIdleMode);
};
