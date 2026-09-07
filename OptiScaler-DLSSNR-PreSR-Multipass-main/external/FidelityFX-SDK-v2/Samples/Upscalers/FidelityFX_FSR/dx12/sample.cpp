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

#include "sample.h"
#include "fsrapirendermodule.h"

#include <cauldron2/dx12/framework/misc/fileio.h>
#include <cauldron2/dx12/rendermodules/rendermoduleregistry.h>
#include <cauldron2/dx12/framework/render/rendermodule.h>
#include <cauldron2/dx12/framework/core/inputmanager.h>
#include <cauldron2/dx12/framework/core/win/framework_win.h>

using namespace cauldron;

// Read in sample-specific configuration parameters.
// Cauldron defaults may also be overridden at this point
void Sample::ParseSampleConfig()
{
    json sampleConfig;
    CauldronAssert(ASSERT_CRITICAL, ParseJsonFile(L"configs\\fsrapiconfig.json", sampleConfig), L"Could not parse JSON file %ls", L"configs\\fsrapiconfig.json");

    // Get the sample configuration
    json configData = sampleConfig["FidelityFX FSR FFXAPI"];

    // Let the framework parse all the "known" options for us
    ParseConfigData(configData);
    // Read in any unknown options

    // Framework config parameters can optionally also be overridden here e.g.
    // m_Config.GPUValidationEnabled = true; // Force GPU Validation

    // If this sample has defined a rendermodule config, we also need to parse the config file for its rendermodule
#if defined(RenderModuleConfig)
    json rmConfig;
    CauldronAssert(ASSERT_CRITICAL, ParseJsonFile(RenderModuleConfig, rmConfig), L"Could not parse JSON file %ls", RenderModuleConfig);

    // Get the render module configuration
    json rmConfigData = rmConfig[RenderModuleConfig];

    // Let the framework parse all the "known" options for us
    ParseConfigData(rmConfigData);
#endif // defined(RenderModuleConfig)
}

void Sample::ParseSampleCmdLine(const wchar_t*)
{
    // Process any command line parameters the sample looks for here
}

// Register sample's render modules so the factory can spawn them
void Sample::RegisterSampleModules()
{
    // Init all pre-registered render modules
    rendermodule::RegisterAvailableRenderModules();

    // Register sample render module
    RenderModuleFactory::RegisterModule<FSRApiRenderModule>("FSRApiRenderModule");
}

// Sample initialization point
void Sample::DoSampleInit()
{

}

// Do any app-specific (global) updates here
// This is called prior to components/render module updates
void Sample::DoSampleUpdates(double)
{
    const InputState& inputState = GetInputManager()->GetInputState();
    FSRApiRenderModule* pFSR = dynamic_cast<FSRApiRenderModule*>(GetFramework()->GetRenderModule("FSRApiRenderModule"));
    
    if (pFSR)
    {
        // Upscale method hotkeys
        constexpr int32_t kUpscalerNative = 0;
        constexpr int32_t kUpscalerFSRAPI = 1;

        // Scale preset hotkeys
        constexpr int32_t kScalePresetNativeAA = 0;
        constexpr int32_t kScalePresetQuality = 1;
        constexpr int32_t kScalePresetCount = 5;

        // Track FSR state - initialize to match the actual starting state from fsrapirendermodule.h
        static bool fsrEnabled = true;
        static int32_t currentPreset = kScalePresetQuality;

        // F5: Toggle FSR method on/off
        // When enabling: set to FSR method with Native AA preset
        // When disabling: set to Native method
        if (inputState.GetKeyUpState(Key_F5))
        {
            fsrEnabled = !fsrEnabled;
            if (fsrEnabled)
            {
                // Enable FSR method first, then set to Native AA preset
                pFSR->SetUpscaleMethodHotkey(kUpscalerFSRAPI);
                currentPreset = kScalePresetNativeAA;
                pFSR->SetScalePresetHotkey(currentPreset);
            }
            else
            {
                // Disable FSR, switch to Native method
                pFSR->SetUpscaleMethodHotkey(kUpscalerNative);
            }
        }

        // F6: Cycle through scale presets
        // If Native method is active: enable FSR method and jump to Quality preset
        // If FSR method is active: cycle through all presets (Native AA -> Quality -> Balanced -> Performance -> Ultra Performance -> Native AA)
        if (inputState.GetKeyUpState(Key_F6))
        {
            if (!fsrEnabled)
            {
                // Currently using Native method, enable FSR first then jump to Quality preset
                pFSR->SetUpscaleMethodHotkey(kUpscalerFSRAPI);
                fsrEnabled = true;
                currentPreset = kScalePresetQuality;
                pFSR->SetScalePresetHotkey(currentPreset);
            }
            else
            {
                // FSR is already enabled, cycle to next preset
                currentPreset = (currentPreset + 1) % kScalePresetCount;
                pFSR->SetScalePresetHotkey(currentPreset);
            }
        }

        // F7: Toggle Frame Interpolation on/off
        if (inputState.GetKeyUpState(Key_F7))
        {
            static bool frameInterpolationEnabled = true;  // Match initial state from fsrapirendermodule.h
            frameInterpolationEnabled = !frameInterpolationEnabled;
            pFSR->SetFrameInterpolationHotkey(frameInterpolationEnabled);
        }
    }
}

// Handle any changes that need to occur due to applicate resize
// NOTE: Cauldron with auto-resize internal resources
void Sample::DoSampleResize(const ResolutionInfo&)
{

}
