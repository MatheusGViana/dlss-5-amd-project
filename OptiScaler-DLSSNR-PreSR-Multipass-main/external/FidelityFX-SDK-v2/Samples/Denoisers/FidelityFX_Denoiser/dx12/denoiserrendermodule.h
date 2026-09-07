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

#pragma once

#include <cauldron2/dx12/framework/render/rendermodule.h>
#include <cauldron2/dx12/framework/core/framework.h>
#include <cauldron2/dx12/framework/core/uimanager.h>

#include <FidelityFX/api/include/ffx_api.hpp>

#include <FidelityFX/denoisers/include/ffx_denoiser.hpp>

namespace cauldron
{
    class Texture;
    class ParameterSet;
    class ResourceView;
    class RootSignature;
    class UIRenderModule;
    class CameraComponent;
    class Entity;
}

class DenoiserRenderModule final : public cauldron::RenderModule
{
public:
    DenoiserRenderModule()
        : RenderModule(L"DenoiserRenderModule")
    {
    }
    ~DenoiserRenderModule() override;

    void Init(const json& initData);
    void EnableModule(bool enabled) override;
    void OnPreFrame() override;

    /**
     * @brief   Setup parameters that the Denoiser context needs this frame and then call the FFX Dispatch.
     */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
     * @brief   Recreate the Denoiser context to resize internal resources. Called by the framework when the resolution changes.
     */
    void OnResize(const cauldron::ResolutionInfo& resInfo) override;

    /**
     * Update the Debug Option UI element.
     */
    void UpdateUI(double deltaTime) override;

    /**
     * @brief   Build UI.
     */
    void BuildUI();

    /**
     * @brief   Returns whether or not Denoiser requires sample-side re-initialization.
     */
    bool NeedsReInit() const
    {
        return m_NeedReInit;
    }

    /**
     * @brief   Clears Denoiser re-initialization flag.
     */
    void ClearReInit()
    {
        m_NeedReInit = false;
    }

    bool UseDominantLightVisibility() const { return m_EnableDominantLightVisibility; }
    bool IsDebugViewEnabled() const { return m_EnableDebugging && m_ShowDebugView; }
    bool IsCheckerboardingEnabled() const { return m_EnableCheckerboarding; }

    /**
     * @brief   Toggles dominant light visibility denoising via hotkey.
     */
    void ToggleDominantLightVisibilityHotkey()
    {
        if (m_DenoiserAvailable)
        {
            m_EnableDominantLightVisibility = !m_EnableDominantLightVisibility;
            m_NeedReInit = true;
        }
    }

    /**
     * @brief   Cycles through view modes via hotkey.
     */
    void CycleViewModeHotkey()
    {
        if (m_DenoiserAvailable)
        {
            m_ViewMode = (m_ViewMode + 1) % static_cast<int32_t>(ViewMode::Count);
        }
    }

    /**
     * @brief   Cycles through RGBA channel combinations via hotkey.
     */
    void CycleRGBAChannelsHotkey()
    {
        if (m_DenoiserAvailable)
        {
            // Treat the 4 booleans as a 4-bit number and increment
            // R=bit0, G=bit1, B=bit2, A=bit3
            static uint32_t channelMask = 0b1111; // Start with all channels enabled (RGBA)

            // Cycle through all 16 combinations (0b0000 to 0b1111)
            channelMask = (channelMask + 1) & 0xF;

            // Update individual channel flags
            m_DebugShowChannelR = (channelMask & 0b0001) != 0;
            m_DebugShowChannelG = (channelMask & 0b0010) != 0;
            m_DebugShowChannelB = (channelMask & 0b0100) != 0;
            m_DebugShowChannelA = (channelMask & 0b1000) != 0;
        }
    }

private:
    void RemapSignals();
    void RemapViewMode();

    bool InitDenoiserContext();
    void DestroyContext();
    bool InitResources();
    bool InitPipelineObjects();
    bool InitContent();

    void ApplyConfiguration();
    void ApplyConfiguration(FfxApiConfigureDenoiserKey key);
    void SetDefaultConfiguration();
    void SetDefaultConfiguration(FfxApiConfigureDenoiserKey key);

    void DispatchPrePass(double deltaTime, cauldron::CommandList* pCmdList);
    void DispatchDenoiser(double deltaTime, cauldron::CommandList* pCmdList, const Vec3& dominantLightDir, const Vec3& dominantLightEmission);
    void DispatchComposition(double deltaTime, cauldron::CommandList* pCmdList, const SceneLightingInformation& sceneLightInfo, uint32_t dominantLightIndex);

    // Settings
    bool m_DenoiserAvailable = {};
    bool m_EnableDebugging   = {};
    bool m_EnableValidation  = {};
    bool m_ShowDebugView     = {};
    
    struct DenoiserConfiguration
    {
        float m_CrossBilateralNormalStrength = {};
        FfxApiFloatBounds m_DebugViewLinearDepthBounds = {};
        float m_DisocclusionThreshold        = {};
        float m_GaussianKernelRelaxation     = {};
        float m_MaxRadiance                  = {};
        float m_RadianceClipStdK             = {};
        float m_StabilityBias                = {};
    };
    DenoiserConfiguration m_DenoiserConfiguration = {};

    FfxApiDenoiserDebugViewMode m_DebugViewMode = FFX_API_DENOISER_DEBUG_VIEW_MODE_OVERVIEW;
    int32_t m_DebugViewport = 0;

    bool m_DebugShowChannelR = true;
    bool m_DebugShowChannelG = true;
    bool m_DebugShowChannelB = true;
    bool m_DebugShowChannelA = true;

    // Input signals
    bool m_EnableDirectDiffuse           = true;
    bool m_EnableDirectSpecular          = true;
    bool m_EnableIndirectDiffuse         = true;
    bool m_EnableIndirectSpecular        = true;
    bool m_EnableDominantLightVisibility = true;
    bool m_EnableAmbientOcclusion        = false;
    bool m_EnableSpecularOcclusion       = false;
    bool m_EnableCheckerboarding         = false;

    enum class ViewMode
    {
        Default,
        InputDefault,
        Direct,
        DirectDiffuse,
        DirectSpecular,
        Indirect,
        IndirectDiffuse,
        IndirectSpecular,
        DominantLightVisibility,
        AmbientOcclusion,
        SpecularOcclusion,
        InputDirect,
        InputDirectDiffuse,
        InputDirectSpecular,
        InputIndirect,
        InputIndirectDiffuse,
        InputIndirectSpecular,
        InputDominantLightVisibility,
        InputAmbientOcclusion,
        InputSpecularOcclusion,
        InputLinearDepth,
        InputMotionVectors,
        InputNormals,
        InputSpecularAlbedo,
        InputDiffuseAlbedo,
        InputSkipSignal,

        Count  // Sentinel value representing total count of view modes
    };
    int32_t m_ViewMode = 0;

    cauldron::Entity* m_pDenoiserCamera = {};
    cauldron::CameraComponent* m_pDenoiserCameraComponent = {};

    const cauldron::Texture* m_pColorTarget                     = {};
    const cauldron::Texture* m_pDepthTarget                     = {};
    const cauldron::Texture* m_pGBufferMotionVectors            = {};

    const cauldron::Texture* m_pLinearDepth                     = {};
    const cauldron::Texture* m_pNormals                         = {};
    const cauldron::Texture* m_pDiffuseAlbedo                   = {};
    const cauldron::Texture* m_pSpecularAlbedo                  = {};
    const cauldron::Texture* m_pSkipSignal                      = {};

    const cauldron::Texture* m_pDebugView                       = {};

    // Noisy input signals
    const cauldron::Texture* m_pDirectDiffuse                   = {};
    const cauldron::Texture* m_pDirectSpecular                  = {};
    const cauldron::Texture* m_pIndirectSpecular                = {};
    const cauldron::Texture* m_pIndirectDiffuse                 = {};
    const cauldron::Texture* m_pDominantLightVisibility         = {};
    const cauldron::Texture* m_pAmbientOcclusion                = {};
    const cauldron::Texture* m_pSpecularOcclusion               = {};
    // Denoised output signals
    const cauldron::Texture* m_pDenoisedDirectDiffuse           = {};
    const cauldron::Texture* m_pDenoisedDirectSpecular          = {};
    const cauldron::Texture* m_pDenoisedIndirectSpecular        = {};
    const cauldron::Texture* m_pDenoisedIndirectDiffuse         = {};
    const cauldron::Texture* m_pDenoisedDominantLightVisibility = {};
    const cauldron::Texture* m_pDenoisedAmbientOcclusion        = {};
    const cauldron::Texture* m_pDenoisedSpecularOcclusion       = {};

    cauldron::SamplerDesc m_BilinearSampler;

    cauldron::RootSignature* m_pPrePassRootSignature = {};
    cauldron::PipelineObject* m_pPrePassPipeline = {};
    cauldron::ParameterSet* m_pPrePassParameterSet = {};
    struct PrePassConstants
    {
        float clipToCamera[16];
        float renderWidth;
        float renderHeight;
        float padding[2];
    };

    cauldron::RootSignature* m_pComposeRootSignature = {};
    cauldron::PipelineObject* m_pComposePipeline = {};

    static constexpr uint32_t m_NumComposeParameterSets = 3;
    cauldron::ParameterSet* m_pComposeParameterSets[m_NumComposeParameterSets];
    cauldron::ParameterSet* m_pCurrentComposeParameterSet = {};
    uint32_t m_pCurrentComposeParameterSetIndex = 0;
    struct ComposeConstants
    {
        Mat4 clipToWorld;
        Mat4 cameraToWorld;

        float directDiffuseContrib;
        float directSpecularContrib;
        float indirectDiffuseContrib;
        float indirectSpecularContrib;

        float skipContrib;
        float rangeMin;
        float rangeMax;
        uint32_t flags;

        Vec4 channelContrib;

        Vec2 invRenderSize;
        uint32_t useDominantLight;
        uint32_t dominantLightIndex;
    };
    enum ComposeFlags
    {
        COMPOSE_DEBUG_MODE = 0x1,
        COMPOSE_DEBUG_USE_RANGE = 0x2,
        COMPOSE_DEBUG_DECODE_SQRT = 0x4,
        COMPOSE_DEBUG_ABS_VALUE = 0x8,
        COMPOSE_DEBUG_DECODE_NORMALS = 0x10,
        COMPOSE_DEBUG_ONLY_FIRST_RESOURCE = 0x20,
        COMPOSE_INPUT_SIGNALS = 0x40
    };

    ffx::Context m_pDenoiserContext = {};

    std::vector<uint64_t> m_DenoiserVersionIds;
    std::vector<const char*> m_DenoiserVersionStrings;
    uint32_t m_SelectedDenoiserVersion = 0;

    Vec3 m_PrevCameraPosition = Vec3(0);

    bool m_NeedReInit = false;
    bool m_ForceReset = false;
};
