#pragma once
#include "SysUtils.h"
#include "IFeature_Dx12.h"
#include <inputs/NVNGX_DLSS.h>

class FeatureProvider_Dx12
{
  public:
    static bool GetFeature(Upscaler upscaler, UINT handleId, NVSDK_NGX_Parameter* parameters,
                           std::unique_ptr<IFeature_Dx12>* feature);

    static bool ChangeFeature(Upscaler upscaler, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                              UINT handleId, NVSDK_NGX_Parameter* parameters, ContextData<IFeature_Dx12>* contextData);
};
