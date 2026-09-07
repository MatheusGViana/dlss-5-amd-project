// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2026 Advanced Micro Devices, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "denoiserrendermodule.h"

#include "Cauldron2/dx12/framework/core/framework.h"
#include "Cauldron2/dx12/framework/core/contentmanager.h"
#include "Cauldron2/dx12/framework/core/scene.h"
#include "Cauldron2/dx12/framework/core/inputmanager.h"
#include "Cauldron2/dx12/framework/core/backend_interface.h"
#include "Cauldron2/dx12/framework/core/components/meshcomponent.h"
#include "Cauldron2/dx12/framework/render/dynamicresourcepool.h"
#include "Cauldron2/dx12/framework/render/dynamicbufferpool.h"
#include "Cauldron2/dx12/framework/render/parameterset.h"
#include "Cauldron2/dx12/framework/render/pipelineobject.h"
#include "Cauldron2/dx12/framework/render/indirectworkload.h"
#include "Cauldron2/dx12/framework/render/rasterview.h"
#include "Cauldron2/dx12/framework/render/profiler.h"
#include "Cauldron2/dx12/framework/render/commandlist.h"
#include "Cauldron2/dx12/framework/shaders/lightingcommon.h"

#include "Cauldron2/dx12/framework/render/dx12/commandlist_dx12.h"

#include <FidelityFX/api/include/dx12/ffx_api_dx12.hpp>
#include <Cauldron2/dx12/framework/render/dx12/device_dx12.h>

using namespace cauldron;

DenoiserRenderModule::~DenoiserRenderModule()
{
    DestroyContext();
}

void DenoiserRenderModule::Init(const json& initData)
{
    CauldronAssert(ASSERT_CRITICAL, GetFramework()->GetConfig()->MinShaderModel >= ShaderModel::SM6_6, L"Error: Denoiser requires SM6_6 or greater");

    InitPipelineObjects();
    InitResources();

    // Logging support
    {
        ffx::ConfigureDescGlobalDebug configureDesc = {};
        configureDesc.effectId  = FFX_API_EFFECT_ID_DENOISER;
        configureDesc.fpMessage = [](uint32_t type, const wchar_t* message)
        {
            if (type == FFX_API_MESSAGE_TYPE_WARNING)
            {
                cauldron::CauldronWarning(message);
            }
            else
            {
                cauldron::CauldronError(message);
            }
        };
        configureDesc.debugLevel = FFX_API_CONFIGURE_GLOBALDEBUG_LEVEL_VERBOSE;

        ffx::ReturnCode retCode = ffx::Configure(configureDesc);
        CauldronAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok, L"Couldn't configure global logging: %d", (uint32_t)retCode);
    }

    // Query Denoiser versions
    {
        uint64_t DenoiserVersionCount = 0;
        ffx::QueryDescGetVersions queryDesc = {};
        queryDesc.createDescType = FFX_API_EFFECT_ID_DENOISER;
        queryDesc.device         = GetDevice()->GetImpl()->DX12Device();
        queryDesc.outputCount    = &DenoiserVersionCount;
        ffx::Query(queryDesc);

        m_DenoiserVersionIds.resize(DenoiserVersionCount);
        m_DenoiserVersionStrings.resize(DenoiserVersionCount);

        queryDesc.versionIds   = m_DenoiserVersionIds.data();
        queryDesc.versionNames = m_DenoiserVersionStrings.data();
        ffx::ReturnCode retCode = ffx::Query(queryDesc);
        CauldronAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok, L"Couldn't query versions: %d", (uint32_t)retCode);
    }

    m_DenoiserAvailable = !m_DenoiserVersionIds.empty();
    if (!m_DenoiserAvailable)
    {
        m_ViewMode = static_cast<int32_t>(ViewMode::InputDefault);
    }

    BuildUI();
    EnableModule(true);
    InitContent();
}

void DenoiserRenderModule::EnableModule(bool enabled)
{
    if (enabled)
    {
        InitDenoiserContext();
        SetModuleEnabled(enabled);
    }
    else
    {
        SetModuleEnabled(enabled);
        DestroyContext();
    }
}

void DenoiserRenderModule::OnPreFrame()
{
    if (NeedsReInit())
    {
        GetDevice()->FlushAllCommandQueues();
        EnableModule(false);
        EnableModule(true);
        ClearReInit();
    }

    RemapViewMode();
}

void DenoiserRenderModule::Execute(double deltaTime, cauldron::CommandList* pCmdList)
{
    GPUScopedProfileCapture sampleMarker(pCmdList, L"FSR Ray Regeneration");

    Vec3 dominantLightDir;
    Vec3 dominantLightEmission;
    uint32_t dominantLightIndex = 0;
    const SceneLightingInformation& sceneLightInfo = GetScene()->GetSceneLightInfo();
    if (UseDominantLightVisibility())
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(sceneLightInfo.LightCount); ++i)
        {
            if (sceneLightInfo.LightInfo[i].Type == static_cast<uint32_t>(LightType::Directional))
            {
                dominantLightDir = sceneLightInfo.LightInfo[i].DirectionRange.getXYZ();
                dominantLightEmission = sceneLightInfo.LightInfo[i].ColorIntensity.getXYZ() * sceneLightInfo.LightInfo[i].ColorIntensity.getW();
                dominantLightIndex = i;
                break;
            }
        }
    }

    DispatchPrePass(deltaTime, pCmdList);
    if (m_DenoiserAvailable)
    {
        DispatchDenoiser(deltaTime, pCmdList, dominantLightDir, dominantLightEmission);
    }
    DispatchComposition(deltaTime, pCmdList, sceneLightInfo, dominantLightIndex);
}

void DenoiserRenderModule::OnResize(const cauldron::ResolutionInfo& resInfo)
{
    m_NeedReInit = true;
}

void DenoiserRenderModule::UpdateUI(double /*deltaTime*/)
{
}

void DenoiserRenderModule::BuildUI()
{
    UISection* uiSection = GetUIManager()->RegisterUIElements("FSR Ray Regeneration", UISectionType::Sample);

    if (!m_DenoiserAvailable)
    {
        auto text = uiSection->RegisterUIElement<UIText>("FSR Ray Regeneration is not supported on this device.");
        text->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f });
        return;
    }

    //--------------------------------------------------------------------------
    // Static Configuration
    //--------------------------------------------------------------------------
    uiSection->RegisterUIElement<UIText>("");
    uiSection->RegisterUIElement<UIText>("Static Configuration:");
    uiSection->RegisterUIElement<UISeparator>();

    uiSection->RegisterUIElement<UICombo>("Version", (int32_t&)m_SelectedDenoiserVersion, m_DenoiserVersionStrings, (bool&)m_DenoiserAvailable, [this](int32_t, int32_t) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable direct diffuse denoising",            (bool&)m_EnableDirectDiffuse,           (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable direct specular denoising",           (bool&)m_EnableDirectSpecular,          (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable indirect diffuse denoising",          (bool&)m_EnableIndirectDiffuse,         (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable indirect specular denoising",         (bool&)m_EnableIndirectSpecular,        (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable dominant light visibility denoising", (bool&)m_EnableDominantLightVisibility, (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable ambient occlusion denoising",         (bool&)m_EnableAmbientOcclusion,        (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable specular occlusion denoising",        (bool&)m_EnableSpecularOcclusion,       (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable checkerboarding",                     (bool&)m_EnableCheckerboarding,         (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable debugging",                           (bool&)m_EnableDebugging,               (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });
    uiSection->RegisterUIElement<UICheckBox>("Enable validation",                          (bool&)m_EnableValidation,              (bool&)m_DenoiserAvailable, [this](int32_t cur, int32_t old) { m_NeedReInit = true; });

    //--------------------------------------------------------------------------
    // Dynamic Configuration
    //--------------------------------------------------------------------------
    uiSection->RegisterUIElement<UIText>("");
    uiSection->RegisterUIElement<UIText>("Dynamic Configuration:");
    uiSection->RegisterUIElement<UISeparator>();

    uiSection->RegisterUIElement<UISlider<float>>("Cross bilateral normal strength", (float&)m_DenoiserConfiguration.m_CrossBilateralNormalStrength, 0.0f, 1.0f, (bool&)m_DenoiserAvailable, [this](float cur, float old) { ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_CROSS_BILATERAL_NORMAL_STRENGTH); });
    uiSection->RegisterUIElement<UISlider<float>>("Disocclusion threshold", (float&)m_DenoiserConfiguration.m_DisocclusionThreshold, 0.01f, 0.05f, (bool&)m_DenoiserAvailable, [this](float cur, float old) { ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_DISOCCLUSION_THRESHOLD); });
    uiSection->RegisterUIElement<UISlider<float>>("Gaussian kernel relaxation", (float&)m_DenoiserConfiguration.m_GaussianKernelRelaxation, 0.0f, 1.0f, (bool&)m_DenoiserAvailable, [this](float cur, float old) { ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_GAUSSIAN_KERNEL_RELAXATION); });
    uiSection->RegisterUIElement<UISlider<float>>("Max radiance", (float&)m_DenoiserConfiguration.m_MaxRadiance, 0.0f, 65504.0f, (bool&)m_DenoiserAvailable, [this](float cur, float old) { ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_MAX_RADIANCE); });
    uiSection->RegisterUIElement<UISlider<float>>("Radiance std Clip", (float&)m_DenoiserConfiguration.m_RadianceClipStdK, 0.0f, 65504.0f, (bool&)m_DenoiserAvailable, [this](float cur, float old) { ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_RADIANCE_CLIP_STD_K); });
    uiSection->RegisterUIElement<UISlider<float>>("Stability bias", (float&)m_DenoiserConfiguration.m_StabilityBias, 0.0f, 1.0f, (bool&)m_DenoiserAvailable, [this](float cur, float old) { ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_STABILITY_BIAS); });
    uiSection->RegisterUIElement<UISlider<float>>("Debug view linear depth max bound", (float&)m_DenoiserConfiguration.m_DebugViewLinearDepthBounds.max, 0.001f, 1024.0f, (bool&)m_DenoiserAvailable, [this](float cur, float old) { ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_DEBUG_VIEW_LINEAR_DEPTH_BOUNDS); });

    //--------------------------------------------------------------------------
    // Display
    //--------------------------------------------------------------------------
    uiSection->RegisterUIElement<UIText>("");
    uiSection->RegisterUIElement<UIText>("Display:");
    uiSection->RegisterUIElement<UISeparator>();
    
    std::vector<const char*> viewModes =
    {
        "Default",
        "Default (Input)",
        "Direct",
        "Direct diffuse",
        "Direct specular",
        "Indirect",
        "Indirect diffuse",
        "Indirect specular",
        "Dominant light visibility",
        "Ambient occlusion",
        "Specular occlusion",
        "Direct (Input)",
        "Direct diffuse (Input)",
        "Direct specular (Input)",
        "Indirect (Input)",
        "Indirect diffuse (Input)",
        "Indirect specular (Input)",
        "Dominant light visibility (Input)",
        "Ambient occlusion (Input)",
        "Specular occlusion (Input)",
        "Linear depth",
        "Motion vectors",
        "Normals",
        "Specular albedo",
        "Diffuse albedo",
        "Skip signal"
    };
    uiSection->RegisterUIElement<UICombo>("View mode", (int32_t&)m_ViewMode, std::move(viewModes), (bool&)m_DenoiserAvailable);
    uiSection->RegisterUIElement<UICheckBox>("R", (bool&)m_DebugShowChannelR, (bool&)m_DenoiserAvailable, nullptr, true, false);
    uiSection->RegisterUIElement<UICheckBox>("G", (bool&)m_DebugShowChannelG, (bool&)m_DenoiserAvailable, nullptr, true, true);
    uiSection->RegisterUIElement<UICheckBox>("B", (bool&)m_DebugShowChannelB, (bool&)m_DenoiserAvailable, nullptr, true, true);
    uiSection->RegisterUIElement<UICheckBox>("A", (bool&)m_DebugShowChannelA, (bool&)m_DenoiserAvailable, nullptr, true, true);
    
    uiSection->RegisterUIElement<UICheckBox>("Show debug view", (bool&)m_ShowDebugView, (bool&)m_EnableDebugging);
    std::vector<const char*> debugViewModes =
    {
        "Overview",
        "Fullscreen Target"
    };
    uiSection->RegisterUIElement<UICombo>("Debug view mode", (int32_t&)m_DebugViewMode, std::move(debugViewModes), (bool&)m_ShowDebugView);
    uiSection->RegisterUIElement<UISlider<int32_t>>("Debug viewport index", (int32_t&)m_DebugViewport, 0, (FFX_API_DENOISER_DEBUG_VIEW_MAX_VIEWPORTS - 1), (bool&)m_ShowDebugView);

    //--------------------------------------------------------------------------

    uiSection->RegisterUIElement<UIText>("");
    uiSection->RegisterUIElement<UIButton>("Reset", (bool&)m_DenoiserAvailable, [this]() { m_ForceReset = true; });
    uiSection->RegisterUIElement<UIText>("");
}

static ffxReturnCode_t AllocateResource(uint32_t effectId,
                                        D3D12_RESOURCE_STATES initialState,
                                        const D3D12_HEAP_PROPERTIES* pHeapProps,
                                        const D3D12_RESOURCE_DESC* pD3DDesc,
                                        const FfxApiResourceDescription* pFfxDesc,
                                        const D3D12_CLEAR_VALUE* pOptimizedClear,
                                        ID3D12Resource** ppD3DResource)
{
    ID3D12Device* device = GetDevice()->GetImpl()->DX12Device();
	HRESULT hr = device->CreateCommittedResource(
		pHeapProps,
		D3D12_HEAP_FLAG_NONE,
		pD3DDesc,
		initialState,
		pOptimizedClear,
		IID_PPV_ARGS(ppD3DResource));

    CAUDRON_LOG_INFO(L"Allocated FFX Resource through callback.");
	return SUCCEEDED(hr) ? FFX_API_RETURN_OK : FFX_API_RETURN_ERROR;
}

static ffxReturnCode_t DellocateResource(uint32_t effectId, ID3D12Resource* pResource)
{
    if (pResource)
    {
        CAUDRON_LOG_INFO(L"Deallocated FFX Resource through callback.");
		pResource->Release();
    }
    return FFX_API_RETURN_OK;
}

ffxReturnCode_t AllocateHeap(uint32_t effectId, const D3D12_HEAP_DESC* pHeapDesc, bool aliasable, ID3D12Heap** ppD3DHeap, uint64_t* pHeapStartOffset)
{
    ID3D12Device* device = GetDevice()->GetImpl()->DX12Device();
    HRESULT hr = device->CreateHeap(
        pHeapDesc,
        IID_PPV_ARGS(ppD3DHeap));

    CAUDRON_LOG_INFO(L"Allocated %s FFX heap with size %.3f MB through callback.", aliasable ? L"aliasable":L"persistent", pHeapDesc->SizeInBytes / 1048576.f);
    return SUCCEEDED(hr) ? FFX_API_RETURN_OK : FFX_API_RETURN_ERROR;
}

ffxReturnCode_t DeallocateHeap(uint32_t effectId, ID3D12Heap* pD3DHeap, uint64_t heapStartOffset, uint64_t heapSize)
{
    if (pD3DHeap)
    {
        CAUDRON_LOG_INFO(L"Deallocated FFX Heap through callback.");
		pD3DHeap->Release();
    }
    return FFX_API_RETURN_OK;
}

void DenoiserRenderModule::RemapSignals()
{
    const int directDiffuse           = static_cast< int >(m_EnableDirectDiffuse);
    const int directSpecular          = static_cast< int >(m_EnableDirectSpecular);
    const int indirectDiffuse         = static_cast< int >(m_EnableIndirectDiffuse);
    const int indirectSpecular        = static_cast< int >(m_EnableIndirectSpecular);
    const int dominantLightVisibility = static_cast< int >(m_EnableDominantLightVisibility);

    if ((directDiffuse + directSpecular + indirectDiffuse + indirectSpecular + dominantLightVisibility) == 0)
    {
        // Remap absence of non-occlusion signals
        m_EnableDirectDiffuse    = true;
        m_EnableIndirectDiffuse  = true;
        m_EnableDirectSpecular   = true;
        m_EnableIndirectSpecular = true;
        cauldron::CauldronWarning(L"Enabled direct/indirect diffuse/specular. "
                                  "Direct/indirect diffuse/specular and dominant light visibility cannot all be disabled at the same time.");
    }
}

void DenoiserRenderModule::RemapViewMode()
{
    // Composition shader does not support combinations of CB and non-CB textures
    // FSR upscaling      does not support combinations of CB and non-CB textures
    if (m_EnableCheckerboarding)
    {
        switch (static_cast< ViewMode >(m_ViewMode))
        {

        case ViewMode::InputDefault:
        case ViewMode::InputDirect:
        case ViewMode::InputDirectDiffuse:
        case ViewMode::InputDirectSpecular:
        case ViewMode::InputIndirect:
        case ViewMode::InputIndirectDiffuse:
        case ViewMode::InputIndirectSpecular:
        case ViewMode::InputDominantLightVisibility:
        case ViewMode::InputAmbientOcclusion:
        case ViewMode::InputSpecularOcclusion:
        {
            // Remap composited and raw input signal view modes
            m_ViewMode = {};
            cauldron::CauldronWarning(L"Composited and raw input signal view modes are not supported when checkerboarding is enabled.");
            break;
        }
        default:
        {
            break;
        }
        }
    }
}

bool DenoiserRenderModule::InitDenoiserContext()
{
    if (!m_DenoiserAvailable)
        return true;

    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();

    ffx::CreateBackendDX12Desc dx12BackendDesc = {};
    dx12BackendDesc.device = GetDevice()->GetImpl()->DX12Device();

    ffx::CreateBackendDX12AllocationCallbacksDesc dx12BackendAllocatorsDesc = {};
    dx12BackendAllocatorsDesc.pfnFfxResourceAllocator = &AllocateResource;
    dx12BackendAllocatorsDesc.pfnFfxResourceDeallocator = &DellocateResource;
    dx12BackendAllocatorsDesc.pfnFfxHeapAllocator = &AllocateHeap;
    dx12BackendAllocatorsDesc.pfnFfxHeapDeallocator = &DeallocateHeap;
    dx12BackendAllocatorsDesc.pfnFfxConstantBufferAllocator = nullptr;

    ffx::CreateContextDescDenoiser denoiserContextDesc = {};
    denoiserContextDesc.version = FFX_DENOISER_VERSION;
    denoiserContextDesc.maxRenderSize = { resInfo.UpscaleWidth, resInfo.UpscaleHeight };
    // Signals
    RemapSignals();
    denoiserContextDesc.signalFlags = FFX_DENOISER_SIGNAL_NONE;
    if (m_EnableDirectDiffuse)
    {
        denoiserContextDesc.signalFlags |= FFX_DENOISER_SIGNAL_DIRECT_DIFFUSE;
    }
    if (m_EnableDirectSpecular)
    {
        denoiserContextDesc.signalFlags |= FFX_DENOISER_SIGNAL_DIRECT_SPECULAR;
    }
    if (m_EnableIndirectDiffuse)
    {
        denoiserContextDesc.signalFlags |= FFX_DENOISER_SIGNAL_INDIRECT_DIFFUSE;
    }
    if (m_EnableIndirectSpecular)
    {
        denoiserContextDesc.signalFlags |= FFX_DENOISER_SIGNAL_INDIRECT_SPECULAR;
    }
    if (m_EnableDominantLightVisibility)
    {
        denoiserContextDesc.signalFlags |= FFX_DENOISER_SIGNAL_DOMINANT_LIGHT_VISIBILITY;
    }
    if (m_EnableAmbientOcclusion)
    {
        denoiserContextDesc.signalFlags |= FFX_DENOISER_SIGNAL_AMBIENT_OCCLUSION;
    }
    if (m_EnableSpecularOcclusion)
    {
        denoiserContextDesc.signalFlags |= FFX_DENOISER_SIGNAL_SPECULAR_OCCLUSION;
    }
    // Reconstruction Mode
    denoiserContextDesc.checkerboardSignalFlags = FFX_DENOISER_SIGNAL_NONE;
    if (m_EnableCheckerboarding)
    {
        if (m_EnableDirectDiffuse)
        {
            denoiserContextDesc.checkerboardSignalFlags |= FFX_DENOISER_SIGNAL_DIRECT_DIFFUSE;
        }
        if (m_EnableDirectSpecular)
        {
            denoiserContextDesc.checkerboardSignalFlags |= FFX_DENOISER_SIGNAL_DIRECT_SPECULAR;
        }
        if (m_EnableIndirectDiffuse)
        {
            denoiserContextDesc.checkerboardSignalFlags |= FFX_DENOISER_SIGNAL_INDIRECT_DIFFUSE;
        }
        if (m_EnableIndirectSpecular)
        {
            denoiserContextDesc.checkerboardSignalFlags |= FFX_DENOISER_SIGNAL_INDIRECT_SPECULAR;
        }
        if (m_EnableDominantLightVisibility)
        {
            denoiserContextDesc.checkerboardSignalFlags |= FFX_DENOISER_SIGNAL_DOMINANT_LIGHT_VISIBILITY;
        }
        if (m_EnableAmbientOcclusion)
        {
            denoiserContextDesc.checkerboardSignalFlags |= FFX_DENOISER_SIGNAL_AMBIENT_OCCLUSION;
        }
        if (m_EnableSpecularOcclusion)
        {
            denoiserContextDesc.checkerboardSignalFlags |= FFX_DENOISER_SIGNAL_SPECULAR_OCCLUSION;
        }
    }
    // Flags
    denoiserContextDesc.flags = {};
    if (m_EnableDebugging)
    {
        denoiserContextDesc.flags |= FFX_DENOISER_ENABLE_DEBUGGING;
    }
    if (m_EnableValidation)
    {
        denoiserContextDesc.flags |= FFX_DENOISER_ENABLE_VALIDATION;
    }

    ffx::CreateContextDescOverrideVersion versionOverride = {};
    versionOverride.versionId = m_DenoiserVersionIds[m_SelectedDenoiserVersion];

    FfxApiEffectMemoryUsage memory = {};
    ffx::QueryDescDenoiserGetGPUMemoryUsage queryMemoryDesc = {};
    queryMemoryDesc.device = dx12BackendDesc.device;
    queryMemoryDesc.maxRenderSize = denoiserContextDesc.maxRenderSize;
    queryMemoryDesc.signalFlags = denoiserContextDesc.signalFlags;
    queryMemoryDesc.checkerboardSignalFlags = denoiserContextDesc.checkerboardSignalFlags;
    queryMemoryDesc.flags = denoiserContextDesc.flags;
    queryMemoryDesc.gpuMemoryUsage = &memory;
    ffx::Query(queryMemoryDesc, versionOverride);
    CAUDRON_LOG_INFO(L"Denoiser version %S Query GPUMemoryUsage VRAM totalUsageInBytes %.3f MB aliasableUsageInBytes %.3f MB", m_DenoiserVersionStrings[m_SelectedDenoiserVersion], memory.totalUsageInBytes / 1048576.f, memory.aliasableUsageInBytes / 1048576.f);

    ffx::ReturnCode retCode = ffx::CreateContext(m_pDenoiserContext, nullptr, denoiserContextDesc, dx12BackendDesc, dx12BackendAllocatorsDesc, versionOverride);
    CauldronAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok, L"Couldn't create the denoiser context: %d", (uint32_t)retCode);

    SetDefaultConfiguration();

    return retCode == ffx::ReturnCode::Ok;
}

void DenoiserRenderModule::DestroyContext()
{
    if (m_pDenoiserContext)
    {
        ffx::DestroyContext(m_pDenoiserContext);
        m_pDenoiserContext = nullptr;
    }
}

bool DenoiserRenderModule::InitResources()
{
    auto renderSizeFn = [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight) {
        desc.Width  = renderingWidth;
        desc.Height = renderingHeight;
    };
    auto displaySizeFn = [](TextureDesc& desc, uint32_t displayWidth, uint32_t displayHeight, uint32_t renderingWidth, uint32_t renderingHeight)
    {
        desc.Width  = displayWidth;
        desc.Height = displayHeight;
    };

    m_pColorTarget             = GetFramework()->GetColorTargetForCallback(GetName());
    m_pDepthTarget             = GetFramework()->GetRenderTexture(L"DepthTarget");
    m_pGBufferMotionVectors    = GetFramework()->GetRenderTexture(L"GBufferMotionVectorRT");

    m_pDiffuseAlbedo           = GetFramework()->GetRenderTexture(L"DenoiserDiffuseAlbedoTarget");
    m_pSpecularAlbedo          = GetFramework()->GetRenderTexture(L"DenoiserSpecularAlbedoTarget");
    m_pNormals                 = GetFramework()->GetRenderTexture(L"DenoiserNormalsTarget");
    m_pSkipSignal              = GetFramework()->GetRenderTexture(L"DenoiserSkipSignalTarget");

    m_pDebugView               = GetFramework()->GetRenderTexture(L"DenoiserDebugViewTarget");

    // Noisy input signals
    m_pDirectDiffuse           = GetFramework()->GetRenderTexture(L"DenoiserDirectDiffuseTarget");
    m_pDirectSpecular          = GetFramework()->GetRenderTexture(L"DenoiserDirectSpecularTarget");
    m_pIndirectDiffuse         = GetFramework()->GetRenderTexture(L"DenoiserIndirectDiffuseTarget");
    m_pIndirectSpecular        = GetFramework()->GetRenderTexture(L"DenoiserIndirectSpecularTarget");
    m_pDominantLightVisibility = GetFramework()->GetRenderTexture(L"DenoiserDominantLightVisibilityTarget");
    m_pAmbientOcclusion        = GetFramework()->GetRenderTexture(L"DenoiserAmbientOcclusionTarget");
    m_pSpecularOcclusion       = GetFramework()->GetRenderTexture(L"DenoiserSpecularOcclusionTarget");
    
    cauldron::TextureDesc desc = m_pDirectDiffuse->GetDesc();
    desc.Flags |= ResourceFlags::AllowUnorderedAccess;
    
    // Denoised output signals
    desc.Name = L"Denoiser_DenoisedDirectDiffuse";
    m_pDenoisedDirectDiffuse = GetDynamicResourcePool()->CreateRenderTexture(&desc, displaySizeFn);

    desc.Name = L"Denoiser_DenoisedDirectSpecular";
    m_pDenoisedDirectSpecular = GetDynamicResourcePool()->CreateRenderTexture(&desc, displaySizeFn);

    desc.Name = L"Denoiser_DenoisedIndirectDiffuse";
    m_pDenoisedIndirectDiffuse = GetDynamicResourcePool()->CreateRenderTexture(&desc, displaySizeFn);

    desc.Name = L"Denoiser_DenoisedIndirectSpecular";
    m_pDenoisedIndirectSpecular = GetDynamicResourcePool()->CreateRenderTexture(&desc, displaySizeFn);

    desc.Name = L"Denoiser_DenoisedDominantLightVisibility";
    desc.Format = ResourceFormat::R8_UNORM;
    m_pDenoisedDominantLightVisibility = GetDynamicResourcePool()->CreateRenderTexture(&desc, displaySizeFn);

    desc.Name = L"Denoiser_DenoisedAmbientOcclusion";
    desc.Format = ResourceFormat::R8_UNORM;
    m_pDenoisedAmbientOcclusion = GetDynamicResourcePool()->CreateRenderTexture(&desc, displaySizeFn);
    
    desc.Name = L"Denoiser_DenoisedSpecularOcclusion";
    desc.Format = ResourceFormat::R8_UNORM;
    m_pDenoisedSpecularOcclusion = GetDynamicResourcePool()->CreateRenderTexture(&desc, displaySizeFn);
    
    desc.Format = ResourceFormat::R32_FLOAT;
    desc.Name = L"Denoiser_LinearDepth";
    m_pLinearDepth = GetDynamicResourcePool()->CreateRenderTexture(&desc, displaySizeFn);

    // SRVs
    m_pPrePassParameterSet->SetTextureSRV(m_pDepthTarget,                             ViewDimension::Texture2D, 0);
    // UAVs
    m_pPrePassParameterSet->SetTextureUAV(m_pLinearDepth,                             ViewDimension::Texture2D, 0);
    m_pPrePassParameterSet->SetTextureUAV(m_pDenoisedDirectDiffuse,                   ViewDimension::Texture2D, 1);
    m_pPrePassParameterSet->SetTextureUAV(m_pDenoisedDirectSpecular,                  ViewDimension::Texture2D, 2);
    m_pPrePassParameterSet->SetTextureUAV(m_pDenoisedIndirectDiffuse,                 ViewDimension::Texture2D, 3);
    m_pPrePassParameterSet->SetTextureUAV(m_pDenoisedIndirectSpecular,                ViewDimension::Texture2D, 4);
    m_pPrePassParameterSet->SetTextureUAV(m_pDenoisedDominantLightVisibility,         ViewDimension::Texture2D, 5);
    m_pPrePassParameterSet->SetTextureUAV(m_pDenoisedAmbientOcclusion,                ViewDimension::Texture2D, 6);
    m_pPrePassParameterSet->SetTextureUAV(m_pDenoisedSpecularOcclusion,               ViewDimension::Texture2D, 7);

    for (uint32_t i = 0; i < m_NumComposeParameterSets; ++i)
    {
        // SRVs
        m_pComposeParameterSets[i]->SetTextureSRV(m_pDenoisedDirectDiffuse,           ViewDimension::Texture2D, 0);
        m_pComposeParameterSets[i]->SetTextureSRV(m_pDenoisedDirectSpecular,          ViewDimension::Texture2D, 1);
        m_pComposeParameterSets[i]->SetTextureSRV(m_pDenoisedIndirectDiffuse,         ViewDimension::Texture2D, 2);
        m_pComposeParameterSets[i]->SetTextureSRV(m_pDenoisedIndirectSpecular,        ViewDimension::Texture2D, 3);
        m_pComposeParameterSets[i]->SetTextureSRV(m_pDenoisedDominantLightVisibility, ViewDimension::Texture2D, 4);
        m_pComposeParameterSets[i]->SetTextureSRV(m_pSkipSignal,                      ViewDimension::Texture2D, 5);
        m_pComposeParameterSets[i]->SetTextureSRV(m_pDiffuseAlbedo,                   ViewDimension::Texture2D, 6);
        m_pComposeParameterSets[i]->SetTextureSRV(m_pSpecularAlbedo,                  ViewDimension::Texture2D, 7);
        m_pComposeParameterSets[i]->SetTextureSRV(m_pNormals,                         ViewDimension::Texture2D, 8);
        m_pComposeParameterSets[i]->SetTextureSRV(m_pDepthTarget,                     ViewDimension::Texture2D, 9);
        // UAVs
        m_pComposeParameterSets[i]->SetTextureUAV(m_pColorTarget,                     ViewDimension::Texture2D, 0);
    }
    m_pCurrentComposeParameterSet = m_pComposeParameterSets[m_pCurrentComposeParameterSetIndex];

    return true;
}

bool DenoiserRenderModule::InitPipelineObjects()
{
    RootSignatureDesc prePassRootSignatureDesc;
    prePassRootSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    prePassRootSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1); // Depth
    prePassRootSignatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);
    prePassRootSignatureDesc.AddTextureUAVSet(1, ShaderBindStage::Compute, 1);
    prePassRootSignatureDesc.AddTextureUAVSet(2, ShaderBindStage::Compute, 1);
    prePassRootSignatureDesc.AddTextureUAVSet(3, ShaderBindStage::Compute, 1);
    prePassRootSignatureDesc.AddTextureUAVSet(4, ShaderBindStage::Compute, 1);
    prePassRootSignatureDesc.AddTextureUAVSet(5, ShaderBindStage::Compute, 1);
    prePassRootSignatureDesc.AddTextureUAVSet(6, ShaderBindStage::Compute, 1);
    prePassRootSignatureDesc.AddTextureUAVSet(7, ShaderBindStage::Compute, 1);
    m_pPrePassRootSignature = RootSignature::CreateRootSignature(L"PrePass_RootSignature", prePassRootSignatureDesc);
    if (!m_pPrePassRootSignature)
        return false;

    PipelineDesc prePassPipelineDesc;
    prePassPipelineDesc.SetRootSignature(m_pPrePassRootSignature);
    ShaderBuildDesc prePassDesc = ShaderBuildDesc::Compute(L"denoiser_prepass.hlsl", L"main", ShaderModel::SM6_6, nullptr);
    prePassPipelineDesc.AddShaderDesc(prePassDesc);
    m_pPrePassPipeline = PipelineObject::CreatePipelineObject(L"PrePass_Pipeline", prePassPipelineDesc);
    if (!m_pPrePassPipeline)
        return false;

    m_pPrePassParameterSet = ParameterSet::CreateParameterSet(m_pPrePassRootSignature);
    m_pPrePassParameterSet->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(PrePassConstants), 0);

    RootSignatureDesc composeRootSignatureDesc;
    composeRootSignatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);
    composeRootSignatureDesc.AddConstantBufferView(1, ShaderBindStage::Compute, 1);
    composeRootSignatureDesc.AddTextureSRVSet(0, ShaderBindStage::Compute, 1); // Direct diffuse
    composeRootSignatureDesc.AddTextureSRVSet(1, ShaderBindStage::Compute, 1); // Direct specular
    composeRootSignatureDesc.AddTextureSRVSet(2, ShaderBindStage::Compute, 1); // Indirect diffuse
    composeRootSignatureDesc.AddTextureSRVSet(3, ShaderBindStage::Compute, 1); // Indirect specular
    composeRootSignatureDesc.AddTextureSRVSet(4, ShaderBindStage::Compute, 1); // Dominant light visibility
    composeRootSignatureDesc.AddTextureSRVSet(5, ShaderBindStage::Compute, 1); // Skip signal
    composeRootSignatureDesc.AddTextureSRVSet(6, ShaderBindStage::Compute, 1); // Diffuse albedo
    composeRootSignatureDesc.AddTextureSRVSet(7, ShaderBindStage::Compute, 1); // Specular albedo
    composeRootSignatureDesc.AddTextureSRVSet(8, ShaderBindStage::Compute, 1); // Normals
    composeRootSignatureDesc.AddTextureSRVSet(9, ShaderBindStage::Compute, 1); // Depth
    //composeRootSignatureDesc.AddStaticSamplers(0, ShaderBindStage::Compute, 1, &m_BilinearSampler);
    composeRootSignatureDesc.AddTextureUAVSet(0, ShaderBindStage::Compute, 1);
    m_pComposeRootSignature = RootSignature::CreateRootSignature(L"Compose_RootSignature", composeRootSignatureDesc);
    if (!m_pComposeRootSignature)
        return false;

    PipelineDesc composePipelineDesc;
    composePipelineDesc.SetRootSignature(m_pComposeRootSignature);
    ShaderBuildDesc composeDesc = ShaderBuildDesc::Compute(L"denoiser_compose.hlsl", L"main", ShaderModel::SM6_6, nullptr);
    composePipelineDesc.AddShaderDesc(composeDesc);
    m_pComposePipeline = PipelineObject::CreatePipelineObject(L"Compose_Pipeline", composePipelineDesc);
    if (!m_pComposePipeline)
        return false;

    for (uint32_t i = 0; i < m_NumComposeParameterSets; ++i)
    {
        m_pComposeParameterSets[i] = ParameterSet::CreateParameterSet(m_pComposeRootSignature);
        m_pComposeParameterSets[i]->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(ComposeConstants), 0);
        m_pComposeParameterSets[i]->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SceneLightingInformation), 1);
    }

    return true;
}

bool DenoiserRenderModule::InitContent()
{
    // Need to create our content on a background thread so proper notifiers can be called
    std::function<void(void*)> createContent = [this](void*)
        {
            CameraComponentData cameraComponentData = {};
            cameraComponentData.Name = L"DenoiserCamera";
            cameraComponentData.Type = CameraType::Perspective;
            cameraComponentData.Zfar = FLT_MAX;
            cameraComponentData.Perspective.AspectRatio = GetFramework()->GetAspectRatio();
            cameraComponentData.Perspective.Yfov = 0.883572936f;

            ContentBlock* pContentBlock = new ContentBlock();

            // Memory backing camera creation
            EntityDataBlock* pCameraDataBlock = new EntityDataBlock();
            pContentBlock->EntityDataBlocks.push_back(pCameraDataBlock);
            pCameraDataBlock->pEntity = new Entity(cameraComponentData.Name.c_str());
            m_pDenoiserCamera = pCameraDataBlock->pEntity;
            CauldronAssert(ASSERT_CRITICAL, m_pDenoiserCamera, L"Could not allocate denoiser camera entity");

            // Calculate transform
            Mat4 lookAt = Mat4::lookAt(Point3(-6.8f, 2.0f, -5.8f), Point3(1.0f, 0.5f, -0.5f), Vec3(0.0f, 1.0f, 0.0f));
            Mat4 transform = InverseMatrix(lookAt);
            m_pDenoiserCamera->SetTransform(transform);

            CameraComponentData* pCameraComponentData = new CameraComponentData(cameraComponentData);
            pCameraDataBlock->ComponentsData.push_back(pCameraComponentData);
            m_pDenoiserCameraComponent = CameraComponentMgr::Get()->SpawnCameraComponent(m_pDenoiserCamera, pCameraComponentData);
            pCameraDataBlock->Components.push_back(m_pDenoiserCameraComponent);
            pContentBlock->ActiveCamera = m_pDenoiserCamera;

            GetContentManager()->StartManagingContent(L"DenoiserRenderModule", pContentBlock, false);

            // We are now ready for use
            SetModuleReady(true);
        };

    // Queue a task to create needed content after setup (but before run)
    Task createContentTask(createContent, nullptr);
    GetFramework()->AddContentCreationTask(createContentTask);

    return true;
}

void DenoiserRenderModule::ApplyConfiguration()
{
    ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_CROSS_BILATERAL_NORMAL_STRENGTH);
    ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_STABILITY_BIAS);
    ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_MAX_RADIANCE);
    ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_RADIANCE_CLIP_STD_K);
    ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_GAUSSIAN_KERNEL_RELAXATION);
    ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_DISOCCLUSION_THRESHOLD);
    ApplyConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_DEBUG_VIEW_LINEAR_DEPTH_BOUNDS);
}

void DenoiserRenderModule::ApplyConfiguration(FfxApiConfigureDenoiserKey key)
{
    ffx::ConfigureDescDenoiserKeyValue configureDesc = {};
    configureDesc.key   = key;
    configureDesc.count = 1u;
    
    switch (key)
    {

    case FFX_API_CONFIGURE_DENOISER_KEY_CROSS_BILATERAL_NORMAL_STRENGTH:
    {
        configureDesc.data = &m_DenoiserConfiguration.m_CrossBilateralNormalStrength;
        const ffx::ReturnCode retCode = ffx::Configure(m_pDenoiserContext, configureDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't configure CrossBilateralNormalStrength parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_STABILITY_BIAS:
    {
        configureDesc.data = &m_DenoiserConfiguration.m_StabilityBias;
        const ffx::ReturnCode retCode = ffx::Configure(m_pDenoiserContext, configureDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't configure StabilityBias parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_MAX_RADIANCE:
    {
        configureDesc.data = &m_DenoiserConfiguration.m_MaxRadiance;
        const ffx::ReturnCode retCode = ffx::Configure(m_pDenoiserContext, configureDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't configure MaxRadiance parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_RADIANCE_CLIP_STD_K:
    {
        configureDesc.data = &m_DenoiserConfiguration.m_RadianceClipStdK;
        const ffx::ReturnCode retCode = ffx::Configure(m_pDenoiserContext, configureDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't configure RadianceClipStdK parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_GAUSSIAN_KERNEL_RELAXATION:
    {
        configureDesc.data = &m_DenoiserConfiguration.m_GaussianKernelRelaxation;
        const ffx::ReturnCode retCode = ffx::Configure(m_pDenoiserContext, configureDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't configure GaussianKernelRelaxation parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_DISOCCLUSION_THRESHOLD:
    {
        configureDesc.data = &m_DenoiserConfiguration.m_DisocclusionThreshold;
        const ffx::ReturnCode retCode = ffx::Configure(m_pDenoiserContext, configureDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't configure DisocclusionThreshold parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_DEBUG_VIEW_LINEAR_DEPTH_BOUNDS:
    {
        configureDesc.data = &m_DenoiserConfiguration.m_DebugViewLinearDepthBounds;
        const ffx::ReturnCode retCode = ffx::Configure(m_pDenoiserContext, configureDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't configure DebugViewLinearDepthBounds parameter: %d", (int)retCode);
        break;
    }
    default:
    {
        CauldronAssert(ASSERT_ERROR, false, L"Unknown configuration parameter key: %d", (int)key);
        break;
    }

    }
}

void DenoiserRenderModule::SetDefaultConfiguration()
{
    SetDefaultConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_CROSS_BILATERAL_NORMAL_STRENGTH);
    SetDefaultConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_STABILITY_BIAS);
    SetDefaultConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_MAX_RADIANCE);
    SetDefaultConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_RADIANCE_CLIP_STD_K);
    SetDefaultConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_GAUSSIAN_KERNEL_RELAXATION);
    SetDefaultConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_DISOCCLUSION_THRESHOLD);
    SetDefaultConfiguration(FFX_API_CONFIGURE_DENOISER_KEY_DEBUG_VIEW_LINEAR_DEPTH_BOUNDS);
}

void DenoiserRenderModule::SetDefaultConfiguration(FfxApiConfigureDenoiserKey key)
{
    ffx::QueryDescDenoiserGetDefaultKeyValue queryDesc = {};
    queryDesc.key   = key;
    queryDesc.count = 1u;
    
    switch (key)
    {

    case FFX_API_CONFIGURE_DENOISER_KEY_CROSS_BILATERAL_NORMAL_STRENGTH:
    {
        queryDesc.data = &m_DenoiserConfiguration.m_CrossBilateralNormalStrength;
        const ffx::ReturnCode retCode = ffx::Query(m_pDenoiserContext, queryDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't query default CrossBilateralNormalStrength parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_STABILITY_BIAS:
    {
        queryDesc.data = &m_DenoiserConfiguration.m_StabilityBias;
        const ffx::ReturnCode retCode = ffx::Query(m_pDenoiserContext, queryDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't query default StabilityBias parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_MAX_RADIANCE:
    {
        queryDesc.data = &m_DenoiserConfiguration.m_MaxRadiance;
        const ffx::ReturnCode retCode = ffx::Query(m_pDenoiserContext, queryDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't query default MaxRadiance parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_RADIANCE_CLIP_STD_K:
    {
        queryDesc.data = &m_DenoiserConfiguration.m_RadianceClipStdK;
        const ffx::ReturnCode retCode = ffx::Query(m_pDenoiserContext, queryDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't query default RadianceClipStdK parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_GAUSSIAN_KERNEL_RELAXATION:
    {
        queryDesc.data = &m_DenoiserConfiguration.m_GaussianKernelRelaxation;
        const ffx::ReturnCode retCode = ffx::Query(m_pDenoiserContext, queryDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't query default GaussianKernelRelaxation parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_DISOCCLUSION_THRESHOLD:
    {
        queryDesc.data = &m_DenoiserConfiguration.m_DisocclusionThreshold;
        const ffx::ReturnCode retCode = ffx::Query(m_pDenoiserContext, queryDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't query default DisocclusionThreshold parameter: %d", (int)retCode);
        break;
    }
    case FFX_API_CONFIGURE_DENOISER_KEY_DEBUG_VIEW_LINEAR_DEPTH_BOUNDS:
    {
        queryDesc.data = &m_DenoiserConfiguration.m_DebugViewLinearDepthBounds;
        const ffx::ReturnCode retCode = ffx::Query(m_pDenoiserContext, queryDesc);
        CauldronAssert(ASSERT_ERROR, retCode == ffx::ReturnCode::Ok, L"Couldn't query default DebugViewLinearDepthBounds parameter: %d", (int)retCode);
        break;
    }
    default:
    {
        CauldronAssert(ASSERT_ERROR, false, L"Unknown configuration parameter key: %d", (int)key);
        break;
    }

    }
}

void DenoiserRenderModule::DispatchPrePass(double deltaTime, cauldron::CommandList* pCmdList)
{
    const ResolutionInfo&   resInfo = GetFramework()->GetResolutionInfo();
    CameraComponent*        pCamera = GetScene()->GetCurrentCamera();

    PrePassConstants constants = {};
    memcpy(constants.clipToCamera, &pCamera->GetInverseProjection(), sizeof(constants.clipToCamera));
    constants.renderWidth  = static_cast<float>(resInfo.RenderWidth);
    constants.renderHeight = static_cast<float>(resInfo.RenderHeight);

    BufferAddressInfo constantsBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(constants), reinterpret_cast<const void*>(&constants));
    m_pPrePassParameterSet->UpdateRootConstantBuffer(&constantsBufferInfo, 0);

    m_pPrePassParameterSet->Bind(pCmdList, m_pPrePassPipeline);
    SetPipelineState(pCmdList, m_pPrePassPipeline);
    const uint32_t numGroupsX = (resInfo.RenderWidth  + 7) / 8;
    const uint32_t numGroupsY = (resInfo.RenderHeight + 7) / 8;
    Dispatch(pCmdList, numGroupsX, numGroupsY, 1);
}

void DenoiserRenderModule::DispatchDenoiser(double deltaTime, cauldron::CommandList* pCmdList, const Vec3& dominantLightDir, const Vec3& dominantLightEmission)
{
    if (NeedsReInit())
        return;

    bool                  reset   = m_ForceReset;
    const ResolutionInfo& resInfo = GetFramework()->GetResolutionInfo();
    CameraComponent*      pCamera = GetScene()->GetCurrentCamera();

    // Needs to persist in the ::ffxDispatch scope.
    ffx::DispatchDescDenoiser dispatchDesc = {};
    dispatchDesc.commandList          = pCmdList->GetImpl()->DX12CmdList();
    dispatchDesc.renderSize.width     = resInfo.RenderWidth;
    dispatchDesc.renderSize.height    = resInfo.RenderHeight;
    dispatchDesc.motionVectorScale.x  = 1.0f; // Motion Vectors are expressed in UV space.
    dispatchDesc.motionVectorScale.y  = 1.0f; // Motion Vectors are expressed in UV space.
    dispatchDesc.motionVectorScale.z  = 1.0f;

    const Vec2 jitterOffsets   = pCamera->GetJitterOffsets();
    dispatchDesc.jitterOffsets = { jitterOffsets.getX(), jitterOffsets.getY() };

    const Vec3 cameraPosition        = pCamera->GetCameraPos();
    const Vec3 cameraPositionDelta   = m_PrevCameraPosition - cameraPosition;
    dispatchDesc.cameraPositionDelta = { cameraPositionDelta.getX(), cameraPositionDelta.getY(), cameraPositionDelta.getZ() };

    // Cauldron: column-major with column vectors
    // FFX:      row-major    with row vectors
    //        == same memory layout
    const Mat4 view       = pCamera->GetView();
    const Mat4 projection = pCamera->GetProjection();
    static_assert(sizeof(FfxApiMatrix4x4) == sizeof(Mat4));
    std::memcpy(&dispatchDesc.view,       &view,       sizeof(FfxApiMatrix4x4));
    std::memcpy(&dispatchDesc.projection, &projection, sizeof(FfxApiMatrix4x4));

    dispatchDesc.linearDepthBounds      = { 0.0f, 1024.0f };

    dispatchDesc.frameIndex             = static_cast< uint32_t >(GetFramework()->GetFrameID());

    dispatchDesc.flags = 0;
    if (reset)
    {
        dispatchDesc.flags |= FFX_DENOISER_DISPATCH_RESET;
    }

    dispatchDesc.linearDepth    = SDKWrapper::ffxGetResourceApi(m_pLinearDepth->GetResource(),          FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchDesc.motionVectors  = SDKWrapper::ffxGetResourceApi(m_pGBufferMotionVectors->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchDesc.normals        = SDKWrapper::ffxGetResourceApi(m_pNormals->GetResource(),              FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchDesc.specularAlbedo = SDKWrapper::ffxGetResourceApi(m_pSpecularAlbedo->GetResource(),       FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    dispatchDesc.diffuseAlbedo  = SDKWrapper::ffxGetResourceApi(m_pDiffuseAlbedo->GetResource(),        FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);

    ffxDispatchDescHeader* prevHeader = &dispatchDesc.header;
    const auto link = [&prevHeader](auto& desc)
    {
        ffxDispatchDescHeader* const nextHeader = &desc.header;
        prevHeader->pNext = nextHeader;
        prevHeader = nextHeader;
    };

    const uint32_t checkerboardOrigin = m_EnableCheckerboarding ? (dispatchDesc.frameIndex & 0b1u) : 0b0u;

    // Need to persist in the ::ffxDispatch scope.
    ffx::DispatchDescDenoiserAmbientOcclusion dispatchDescAmbientOcclusion = {};
    if (m_EnableAmbientOcclusion)
    {
        dispatchDescAmbientOcclusion.signal.input               = SDKWrapper::ffxGetResourceApi(m_pAmbientOcclusion->GetResource(),                FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchDescAmbientOcclusion.signal.output              = SDKWrapper::ffxGetResourceApi(m_pDenoisedAmbientOcclusion->GetResource(),        FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        dispatchDescAmbientOcclusion.signal.checkerboardOrigin  = checkerboardOrigin;
    
        link(dispatchDescAmbientOcclusion);
    }

    // Need to persist in the ::ffxDispatch scope.
    ffx::DispatchDescDenoiserDirectDiffuse dispatchDescDirectDiffuse = {};
    if (m_EnableDirectDiffuse)
    {
        dispatchDescDirectDiffuse.signal.input                  = SDKWrapper::ffxGetResourceApi(m_pDirectDiffuse->GetResource(),                   FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchDescDirectDiffuse.signal.output                 = SDKWrapper::ffxGetResourceApi(m_pDenoisedDirectDiffuse->GetResource(),           FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        dispatchDescDirectDiffuse.signal.checkerboardOrigin     = checkerboardOrigin;

        link(dispatchDescDirectDiffuse);
    }

    // Need to persist in the ::ffxDispatch scope.
    ffx::DispatchDescDenoiserDirectSpecular dispatchDescDirectSpecular = {};
    if (m_EnableDirectSpecular)
    {
        dispatchDescDirectSpecular.signal.input                 = SDKWrapper::ffxGetResourceApi(m_pDirectSpecular->GetResource(),                  FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchDescDirectSpecular.signal.output                = SDKWrapper::ffxGetResourceApi(m_pDenoisedDirectSpecular->GetResource(),          FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        dispatchDescDirectSpecular.signal.checkerboardOrigin    = checkerboardOrigin;

        link(dispatchDescDirectSpecular);
    }

    // Need to persist in the ::ffxDispatch scope.
    ffx::DispatchDescDenoiserDominantLight dispatchDescDominantLight = {};
    if (m_EnableDominantLightVisibility)
    {
        dispatchDescDominantLight.emission                      = { dominantLightEmission.getX(), dominantLightEmission.getY(), dominantLightEmission.getZ() };
        dispatchDescDominantLight.direction                     = { -dominantLightDir.getX(), -dominantLightDir.getY(), -dominantLightDir.getZ() };
        dispatchDescDominantLight.angularRadius                 = {},
        dispatchDescDominantLight.signal.input                  = SDKWrapper::ffxGetResourceApi(m_pDominantLightVisibility->GetResource(),         FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchDescDominantLight.signal.output                 = SDKWrapper::ffxGetResourceApi(m_pDenoisedDominantLightVisibility->GetResource(), FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        dispatchDescDominantLight.signal.checkerboardOrigin     = checkerboardOrigin;
        
        link(dispatchDescDominantLight);
    }

    // Need to persist in the ::ffxDispatch scope.
    ffx::DispatchDescDenoiserIndirectDiffuse dispatchDescIndirectDiffuse = {};
    if (m_EnableIndirectDiffuse)
    {
        dispatchDescIndirectDiffuse.signal.input                = SDKWrapper::ffxGetResourceApi(m_pIndirectDiffuse->GetResource(),                 FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchDescIndirectDiffuse.signal.output               = SDKWrapper::ffxGetResourceApi(m_pDenoisedIndirectDiffuse->GetResource(),         FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        dispatchDescIndirectDiffuse.signal.checkerboardOrigin   = checkerboardOrigin;
    
        link(dispatchDescIndirectDiffuse);
    }

    // Need to persist in the ::ffxDispatch scope.
    ffx::DispatchDescDenoiserIndirectSpecular dispatchDescIndirectSpecular = {};
    if (m_EnableIndirectSpecular)
    {
        dispatchDescIndirectSpecular.signal.input               = SDKWrapper::ffxGetResourceApi(m_pIndirectSpecular->GetResource(),                FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchDescIndirectSpecular.signal.output              = SDKWrapper::ffxGetResourceApi(m_pDenoisedIndirectSpecular->GetResource(),        FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        dispatchDescIndirectSpecular.signal.checkerboardOrigin  = checkerboardOrigin;
    
        link(dispatchDescIndirectSpecular);
    }

    // Need to persist in the ::ffxDispatch scope.
    ffx::DispatchDescDenoiserSpecularOcclusion dispatchDescSpecularOcclusion = {};
    if (m_EnableSpecularOcclusion)
    {
        dispatchDescSpecularOcclusion.signal.input              = SDKWrapper::ffxGetResourceApi(m_pSpecularOcclusion->GetResource(),               FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
        dispatchDescSpecularOcclusion.signal.output             = SDKWrapper::ffxGetResourceApi(m_pDenoisedSpecularOcclusion->GetResource(),       FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        dispatchDescSpecularOcclusion.signal.checkerboardOrigin = checkerboardOrigin;
    
        link(dispatchDescSpecularOcclusion);
    }

    // Need to persist in the ::ffxDispatch scope.
    ffx::DispatchDescDenoiserDebugView dispatchDescDebugView = {};
    if (m_EnableDebugging && m_ShowDebugView)
    {
        dispatchDescDebugView.output                            = SDKWrapper::ffxGetResourceApi(m_pDebugView->GetResource(),                       FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        dispatchDescDebugView.outputSize                        = { m_pDebugView->GetDesc().Width, m_pDebugView->GetDesc().Height };
        dispatchDescDebugView.mode                              = m_DebugViewMode;
        dispatchDescDebugView.viewportIndex                     = static_cast< uint32_t >(m_DebugViewport);

        link(dispatchDescDebugView);
    }

    // ffx::Dispatch cannot be used because it will reset pNext.
    ffxReturnCode_t ret = ::ffxDispatch(&m_pDenoiserContext, &dispatchDesc.header);
    CAULDRON_ASSERT(ret == FFX_API_RETURN_OK);

    // Reset all descriptor heaps
    SetAllResourceViewHeaps(pCmdList);
    m_ForceReset = false;
    m_PrevCameraPosition = cameraPosition;
}

void DenoiserRenderModule::DispatchComposition(double deltaTime, cauldron::CommandList* pCmdList, const SceneLightingInformation& sceneLightInfo, uint32_t dominantLightIndex)
{
    const ResolutionInfo&   resInfo = GetFramework()->GetResolutionInfo();
    CameraComponent*        pCamera = GetScene()->GetCurrentCamera();

    // triple buffered to work around CPU/GPU overlap
    m_pCurrentComposeParameterSetIndex = (m_pCurrentComposeParameterSetIndex + 1) % m_NumComposeParameterSets;
    m_pCurrentComposeParameterSet = m_pComposeParameterSets[m_pCurrentComposeParameterSetIndex];

    m_pCurrentComposeParameterSet->SetTextureSRV(m_pDenoisedDirectDiffuse, ViewDimension::Texture2D, 0);
    m_pCurrentComposeParameterSet->SetTextureSRV(m_pDenoisedDirectSpecular, ViewDimension::Texture2D, 1);
    m_pCurrentComposeParameterSet->SetTextureSRV(m_pDenoisedIndirectDiffuse, ViewDimension::Texture2D, 2);
    m_pCurrentComposeParameterSet->SetTextureSRV(m_pDenoisedIndirectSpecular, ViewDimension::Texture2D, 3);
    m_pCurrentComposeParameterSet->SetTextureSRV(m_pDenoisedDominantLightVisibility, ViewDimension::Texture2D, 4);

    ComposeConstants constants = {};
    constants.channelContrib = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    constants.flags = 0;

    const ViewMode viewMode  = static_cast<ViewMode>(m_ViewMode);
    switch (viewMode)
    {
    default:
    case ViewMode::Default:
        constants.directDiffuseContrib = 1.0f;
        constants.directSpecularContrib = 1.0f;
        constants.indirectDiffuseContrib = 1.0f;
        constants.indirectSpecularContrib = 1.0f;
        constants.skipContrib = 1.0f;
        break;
    case ViewMode::InputDefault:
        constants.directDiffuseContrib    = 1.0f;
        constants.directSpecularContrib   = 1.0f;
        constants.indirectDiffuseContrib  = 1.0f;
        constants.indirectSpecularContrib = 1.0f;
        constants.skipContrib             = 1.0f;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDirectDiffuse, ViewDimension::Texture2D, 0);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDirectSpecular, ViewDimension::Texture2D, 1);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pIndirectDiffuse, ViewDimension::Texture2D, 2);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pIndirectSpecular, ViewDimension::Texture2D, 3);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDominantLightVisibility, ViewDimension::Texture2D, 4);
        break;
    case ViewMode::Direct:
        constants.directDiffuseContrib = 1.0f;
        constants.directSpecularContrib = 1.0f;
        break;
    case ViewMode::DirectDiffuse:
        constants.directDiffuseContrib = 1.0f;
        break;
    case ViewMode::DirectSpecular:
        constants.directSpecularContrib = 1.0f;
        break;
    case ViewMode::Indirect:
        constants.indirectDiffuseContrib = 1.0f;
        constants.indirectSpecularContrib = 1.0f;
        break;
    case ViewMode::IndirectDiffuse:
        constants.indirectDiffuseContrib = 1.0f;
        break;
    case ViewMode::IndirectSpecular:
        constants.indirectSpecularContrib = 1.0f;
        break;
    case ViewMode::DominantLightVisibility:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_USE_RANGE;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.rangeMin = 0.0f;
        constants.rangeMax = 1.0f;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDenoisedDominantLightVisibility, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::AmbientOcclusion:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_USE_RANGE;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.rangeMin = 0.0f;
        constants.rangeMax = 1.0f;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDenoisedAmbientOcclusion, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::SpecularOcclusion:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_USE_RANGE;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.rangeMin = 0.0f;
        constants.rangeMax = 1.0f;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDenoisedSpecularOcclusion, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::InputDirect:
        constants.directDiffuseContrib = 1.0f;
        constants.directSpecularContrib = 1.0f;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDirectDiffuse, ViewDimension::Texture2D, 0);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDirectSpecular, ViewDimension::Texture2D, 1);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDominantLightVisibility, ViewDimension::Texture2D, 4);
        break;
    case ViewMode::InputDirectDiffuse:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDirectDiffuse, ViewDimension::Texture2D, 0);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDominantLightVisibility, ViewDimension::Texture2D, 4);
        break;
    case ViewMode::InputDirectSpecular:
        constants.directSpecularContrib = 1.0f;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDirectSpecular, ViewDimension::Texture2D, 1);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDominantLightVisibility, ViewDimension::Texture2D, 4);
        break;
    case ViewMode::InputIndirect:
        constants.indirectDiffuseContrib  = 1.0f;
        constants.indirectSpecularContrib = 1.0f;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pIndirectDiffuse, ViewDimension::Texture2D, 2);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pIndirectSpecular, ViewDimension::Texture2D, 3);
        break;
    case ViewMode::InputIndirectDiffuse:
        constants.indirectDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pIndirectDiffuse, ViewDimension::Texture2D, 2);
        break;
    case ViewMode::InputIndirectSpecular:
        constants.indirectSpecularContrib = 1.0f;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pIndirectSpecular, ViewDimension::Texture2D, 3);
        break;
    case ViewMode::InputDominantLightVisibility:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_USE_RANGE;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        constants.rangeMin = 0.0f;
        constants.rangeMax = 1024.0f;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDominantLightVisibility, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::InputAmbientOcclusion:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_USE_RANGE;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        constants.rangeMin = 0.0f;
        constants.rangeMax = 1.0f;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pAmbientOcclusion, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::InputSpecularOcclusion:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_USE_RANGE;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.flags |= COMPOSE_INPUT_SIGNALS;
        constants.rangeMin = 0.0f;
        constants.rangeMax = 1.0f;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pSpecularOcclusion, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::InputLinearDepth:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_USE_RANGE;
        constants.flags |= COMPOSE_DEBUG_ABS_VALUE;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.rangeMin = m_DenoiserConfiguration.m_DebugViewLinearDepthBounds.min;
        constants.rangeMax = m_DenoiserConfiguration.m_DebugViewLinearDepthBounds.max;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pLinearDepth, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::InputMotionVectors:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pGBufferMotionVectors, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::InputNormals:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_DECODE_NORMALS;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pNormals, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::InputSpecularAlbedo:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_DECODE_SQRT;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pSpecularAlbedo, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::InputDiffuseAlbedo:
        constants.directDiffuseContrib = 1.0f;
        constants.flags |= COMPOSE_DEBUG_MODE;
        constants.flags |= COMPOSE_DEBUG_DECODE_SQRT;
        constants.flags |= COMPOSE_DEBUG_ONLY_FIRST_RESOURCE;
        constants.channelContrib = Vec4((float)m_DebugShowChannelR, (float)m_DebugShowChannelG, (float)m_DebugShowChannelB, (float)m_DebugShowChannelA);
        m_pCurrentComposeParameterSet->SetTextureSRV(m_pDiffuseAlbedo, ViewDimension::Texture2D, 0);
        break;
    case ViewMode::InputSkipSignal:
        constants.skipContrib = 1.0f;
        break;
    }

    constants.useDominantLight = static_cast<uint32_t>(UseDominantLightVisibility());
    constants.dominantLightIndex = dominantLightIndex;

    std::memcpy(&constants.clipToWorld,   &pCamera->GetInverseViewProjection(), sizeof(constants.clipToWorld));
    std::memcpy(&constants.cameraToWorld, &pCamera->GetInverseView(),           sizeof(constants.cameraToWorld));
    constants.invRenderSize[0] = 1.0f / resInfo.RenderWidth;
    constants.invRenderSize[1] = 1.0f / resInfo.RenderHeight;

    BufferAddressInfo constantsBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(constants), reinterpret_cast<const void*>(&constants));
    m_pCurrentComposeParameterSet->UpdateRootConstantBuffer(&constantsBufferInfo, 0);

    BufferAddressInfo lightingBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(sceneLightInfo), reinterpret_cast<const void*>(&sceneLightInfo));
    m_pCurrentComposeParameterSet->UpdateRootConstantBuffer(&lightingBufferInfo, 1);

    m_pCurrentComposeParameterSet->Bind(pCmdList, m_pComposePipeline);
    SetPipelineState(pCmdList, m_pComposePipeline);
    const uint32_t numGroupsX = (resInfo.RenderWidth  + 7) / 8;
    const uint32_t numGroupsY = (resInfo.RenderHeight + 7) / 8;
    Dispatch(pCmdList, numGroupsX, numGroupsY, 1);
}
