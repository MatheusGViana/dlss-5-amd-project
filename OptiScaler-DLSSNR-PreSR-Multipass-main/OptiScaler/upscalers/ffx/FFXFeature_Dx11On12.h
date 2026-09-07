#pragma once
#include <upscalers/IFeature_Dx11wDx12.h>

class FFXFeatureDx11on12 : public IFeature_Dx11wDx12
{
  public:
    Upscaler GetUpscalerType() const final { return Upscaler::FFX_on12; }

    FFXFeatureDx11on12(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters);
};
