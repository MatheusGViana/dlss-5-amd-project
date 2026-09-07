#include <pch.h>

#include "FSR2Feature_Dx11On12.h"
#include <upscalers/fsr2/FSR2Feature_Dx12.h>

FSR2FeatureDx11on12::FSR2FeatureDx11on12(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters)
    : IFeature_Dx11wDx12(InHandleId, InParameters), IFeature_Dx11(InHandleId, InParameters),
      IFeature(InHandleId, InParameters)
{
    dx12Feature = std::make_unique<FSR2FeatureDx12>(InHandleId, InParameters);
}
