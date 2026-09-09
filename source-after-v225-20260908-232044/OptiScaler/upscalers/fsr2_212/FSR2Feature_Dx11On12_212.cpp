#include <pch.h>

#include "FSR2Feature_Dx11On12_212.h"
#include <upscalers/fsr2_212/FSR2Feature_Dx12_212.h>

FSR2FeatureDx11on12_212::FSR2FeatureDx11on12_212(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters)
    : IFeature_Dx11wDx12(InHandleId, InParameters), IFeature_Dx11(InHandleId, InParameters),
      IFeature(InHandleId, InParameters)
{
    dx12Feature = std::make_unique<FSR2FeatureDx12_212>(InHandleId, InParameters);
}
