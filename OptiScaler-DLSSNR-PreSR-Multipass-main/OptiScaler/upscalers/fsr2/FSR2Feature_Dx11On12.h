#pragma once
#include <upscalers/IFeature_Dx11wDx12.h>

class FSR2FeatureDx11on12 : public IFeature_Dx11wDx12
{
  public:
    Upscaler GetUpscalerType() const final { return Upscaler::FSR22_on12; }

    FSR2FeatureDx11on12(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters);
};
