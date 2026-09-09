#include <pch.h>

#include "XeSSFeature_Dx11on12.h"
#include "XeSSFeature_Dx12.h"

XeSSFeatureDx11on12::XeSSFeatureDx11on12(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters)
    : IFeature_Dx11wDx12(InHandleId, InParameters), IFeature_Dx11(InHandleId, InParameters),
      IFeature(InHandleId, InParameters)
{
    dx12Feature = std::make_unique<XeSSFeatureDx12>(InHandleId, InParameters);
}
