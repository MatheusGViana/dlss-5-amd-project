#pragma once
#include <upscalers/IFeature_Dx11wDx12.h>

class XeSSFeatureDx11on12 : public IFeature_Dx11wDx12
{
  public:
    Upscaler GetUpscalerType() const final { return Upscaler::XeSS_on12; }

    XeSSFeatureDx11on12(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters);
};
