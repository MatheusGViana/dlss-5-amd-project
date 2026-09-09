#pragma once
#include "SysUtils.h"
#include <NVNGX_Parameter.h>
#include <upscalers/IFeature_Dx12.h>

namespace FSR3FG
{
void HookFSR3FGExeInputs();
void HookFSR3FGInputs();
void ffxPresentCallback();
void SetUpscalerInputs(ID3D12GraphicsCommandList* InCmdList, NVSDK_NGX_Parameter* InParameters, IFeature_Dx12* feature);
}; // namespace FSR3FG
