#pragma once

#include "SysUtils.h"

#include "IFeature_Dx11.h"

#include <inputs/NVNGX_DLSS.h>

class FeatureProvider_Dx11
{
  public:
    static bool GetFeature(Upscaler upscaler, UINT handleId, NVSDK_NGX_Parameter* parameters,
                           std::unique_ptr<IFeature_Dx11>* feature);

    static bool ChangeFeature(Upscaler upscaler, ID3D11Device* device, ID3D11DeviceContext* cmdList, UINT handleId,
                              NVSDK_NGX_Parameter* parameters, ContextData<IFeature_Dx11>* contextData);
};
