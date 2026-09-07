#pragma once

#include "ffx_api.h"

#define PASSTHRU_RETURN_CODE (ffxReturnCode_t) 0xFFFFFFFF
const size_t scContext = 0x13375CC;
const size_t fgContext = 0x133757C;
const uint32_t rcContinue = 100;

ffxReturnCode_t ffxCreateContext_Dx12FG(ffxContext* context, ffxCreateContextDescHeader* desc,
                                        const ffxAllocationCallbacks* memCb);
ffxReturnCode_t ffxDestroyContext_Dx12FG(ffxContext* context, const ffxAllocationCallbacks* memCb);
ffxReturnCode_t ffxConfigure_Dx12FG(ffxContext* context, ffxConfigureDescHeader* desc);
ffxReturnCode_t ffxQuery_Dx12FG(ffxContext* context, ffxQueryDescHeader* desc);
ffxReturnCode_t ffxDispatch_Dx12FG(ffxContext* context, ffxDispatchDescHeader* desc);
void ffxPresentCallback();
