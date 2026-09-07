#pragma once
#include <upscalers/IFeature_Dx11wDx12.h>

class FSR2FeatureDx11on12_212 : public IFeature_Dx11wDx12
{
  public:
    Upscaler GetUpscalerType() const final { return Upscaler::FSR21_on12; }

    FSR2FeatureDx11on12_212(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters);
};
