<!-- @page page_techniques_frame-interpolation-api AMD FSR™ Frame Generation API -->

<h1>AMD FSR™ Frame Generation API</h1>

<h2>Table of contents</h2>

- [Introduction](#introduction)
- [Integration](#integration)
    - [Memory Usage](#memory-usage)
    - [New APIs](#new-apis)
    - [Create frame generation context](#create-frame-generation-context)
    - [Configure frame generation](#configure-frame-generation)
    - [Distortion Field](#distortion-field)
    - [UI Composition](#ui-composition)
    - [Dispatch frame generation preparation](#dispatch-frame-generation-preparation)
    - [Dispatch frame generation](#dispatch-frame-generation)
    - [Input color requirements](#input-color-requirements)
    - [Letterboxing or pillarboxing](#letterboxing-or-pillarboxing)
    - [Shutdown](#shutdown)
    - [Thread safety](#thread-safety)
    - [Resource Lifetime](#resource-lifetime)
    - [Debug Checker](#debug-checker)
    - [Debug output](#debug-output)
- [See also](#see-also)

<h2>Introduction</h2>

This document describes the AMD FSR™ API constructs used to integrate **AMD FSR™ Frame Generation** and related swapchain functionality. Use it together with the [Introduction to the AMD FSR™ API](../getting-started/ffx-api.md) for context creation, querying, dispatching, and version selection.

<h2>Integration</h2>

**AMD FSR™ Frame Generation** should be integrated using the [AMD FSR™ API](../getting-started/ffx-api.md). The sections below describe API constructs specific to AMD FSR™ Frame Generation. This API is common across all versions of AMD FSR™ Frame Generation, with any differences noted on the version-specific pages.

<h3>Memory Usage</h3>

An application can get amount of GPU local memory required by AMD FSR™ Frame Generation after context creation by calling [`ffxQuery`](../../api/include/ffx_api.h#L160) with the valid context and [`ffxQueryDescFrameGenerationGetGPUMemoryUsage`](../../framegeneration/include/ffx_framegeneration.h#L174).

An application can get GPU local memory required by default AMD FSR™ Frame Generation version before context creation by calling [`ffxQuery`](../../api/include/ffx_api.h#L160) with `NULL` context and filling out [`ffxQueryDescFrameGenerationGetGPUMemoryUsageV2`](../../framegeneration/include/ffx_framegeneration.h#L217). To get the memory requirement info for a different AMD FSR™ Frame Generation version, additionally link [`ffxOverrideVersion`](../../api/include/ffx_api.h#L109). 

See code examples how to call [Query](../getting-started/ffx-api.md#Query).

<h3>New APIs</h3>

As of SDK 2.1 it is necessary to create and fill in a [`ffxCreateContextDescFrameGenerationVersion`](../../framegeneration/include/ffx_framegeneration.h#L273), setting the [`version`](../../framegeneration/include/ffx_framegeneration.h#L276) field to [`FFX_FRAMEGENERATION_VERSION`](../../framegeneration/include/ffx_framegeneration.h#L33) and link this in the header.pNext field of the above [`ffxCreateContextDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L74) structure. This is used to ensure compatibility between different versions of the API.

As of SDK 2.1 both [`ffxDispatchDescFrameGenerationPrepare`](../../framegeneration/include/ffx_framegeneration.h#L136) and [`ffxDispatchDescFrameGenerationPrepareCameraInfo`](../../framegeneration/include/ffx_framegeneration.h#L205) are deprecated. Use a [`ffxDispatchDescFrameGenerationPrepareV2`](../../framegeneration/include/ffx_framegeneration.h#L231) structure instead.

The [`reset`](../../framegeneration/include/ffx_framegeneration.h#L242) field of [`ffxDispatchDescFrameGenerationPrepareV2`](../../framegeneration/include/ffx_framegeneration.h#L231) is now used by the `AMD FSR™ Frame Generation` implementations and behaves the same as providing disjoint `frameID`, unlike the previous `unused_reset` field which was ignored.

<h3>Create frame generation context</h3>

In order to use frame generation first call [`ffxCreateContext`](../../api/include/ffx_api.h#L142) with a description for frame generation and a backend description.

The [`ffxCreateContextDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L74) structure contains configuration data:
 * A set of initialization flags
 * The maximum resolution the rendering will be performed at
 * The resolution of the resources that will get interpolated
 * The format of the resources that will get interpolated

The initialization flags are provided though the [`FfxApiCreateContextFramegenerationFlags`](../../framegeneration/include/ffx_framegeneration.h#L38) enumeration:

| Flag                                            | Note                                                                                                                         |
|-------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------|
| [`FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT`](../../framegeneration/include/ffx_framegeneration.h#L40) | A bit indicating whether the context supports async. dispatch for frame-generation workloads. |
| [`FFX_FRAMEGENERATION_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS`](../../framegeneration/include/ffx_framegeneration.h#L41) | A bit indicating if the motion vectors are rendered at display resolution. |
| [`FFX_FRAMEGENERATION_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION`](../../framegeneration/include/ffx_framegeneration.h#L42) | A bit indicating that the motion vectors have the jittering pattern applied to them. |
| [`FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED`](../../framegeneration/include/ffx_framegeneration.h#L43) | A bit indicating that the input depth buffer data provided is inverted [1..0].A bit indicating the depth buffer is inverted. |
| [`FFX_FRAMEGENERATION_ENABLE_DEPTH_INFINITE`](../../framegeneration/include/ffx_framegeneration.h#L44) | A bit indicating that the input depth buffer data provided is using an infinite far plane.                                   |
| [`FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE`](../../framegeneration/include/ffx_framegeneration.h#L45) | A bit indicating if the input color data provided to all inputs is using a high-dynamic range.                                                                    |
| [`FFX_FRAMEGENERATION_ENABLE_DEBUG_CHECKING`](../../framegeneration/include/ffx_framegeneration.h#L46) | A bit indicating that the runtime should check some API values and report issues.                                             |

Example using the C++ helpers:

```C++
ffx::Context frameGenContext;
ffx::CreateBackendDX12Desc backendDesc{};
backendDesc.device = GetDevice()->DX12Device();

ffx::CreateContextDescFrameGenerationVersion createFgVersion{};
createFgVersion.version = FFX_FRAMEGENERATION_VERSION;

ffx::CreateContextDescFrameGeneration createFg{};
createFg.displaySize = {resInfo.DisplayWidth, resInfo.DisplayHeight};
createFg.maxRenderSize = {resInfo.DisplayWidth, resInfo.DisplayHeight};
createFg.flags = FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE;

if (m_EnableAsyncCompute)
    createFg.flags |= FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT;

createFg.backBufferFormat = SDKWrapper::GetFfxSurfaceFormat(GetFramework()->GetSwapChain()->GetSwapChainFormat());
ffx::ReturnCode retCode = ffx::CreateContext(frameGenContext, nullptr, createFg, createFgVersion, backendDesc);
```

<h3>Configure frame generation</h3>

Configure frame generation by filling out the [`ffxConfigureDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L115) structure with the required arguments and calling [`ffxConfigure`](../../api/include/ffx_api.h#L154).

This must be called once per frame. The frame ID must increment by exactly 1 each frame. Any other difference between consecutive frames will reset frame generation logic.

| [`ffxConfigureDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L115) member | Note                                                                                                                               |
|---------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------|
| [`swapChain`](../../framegeneration/include/ffx_framegeneration.h#L118)                                           | The swapchain to use with frame generation.                                                                             |
| [`presentCallback`](../../framegeneration/include/ffx_framegeneration.h#L119)                                           | A UI composition callback to call when finalizing the frame image.                                                                             |
| [`presentCallbackUserContext`](../../framegeneration/include/ffx_framegeneration.h#L120)                                           | A pointer to be passed to the UI composition callback.                                                                             |
| [`frameGenerationCallback`](../../framegeneration/include/ffx_framegeneration.h#L121)                                           | The frame generation callback to use to generate a frame.                                                                             |
| [`frameGenerationCallbackUserContext`](../../framegeneration/include/ffx_framegeneration.h#L122)                                           | A pointer to be passed to the frame generation callback.                                                                             |
| [`frameGenerationEnabled`](../../framegeneration/include/ffx_framegeneration.h#L123)                                           | Sets the state of frame generation. Set to false to disable frame generation.                                                                             |
| [`allowAsyncWorkloads`](../../framegeneration/include/ffx_framegeneration.h#L124)                                           | Sets the state of async workloads. Set to true to enable generation work on async compute.                                                                             |
| [`HUDLessColor`](../../framegeneration/include/ffx_framegeneration.h#L125)                                           | The hudless back buffer image to use for UI extraction from backbuffer resource. May be empty.                                                                             |
| [`flags`](../../framegeneration/include/ffx_framegeneration.h#L126)                                           | Zero or combination of flags from [`FfxApiDispatchFramegenerationFlags`](../../framegeneration/include/ffx_framegeneration.h#L52).                                                                             |
| [`onlyPresentGenerated`](../../framegeneration/include/ffx_framegeneration.h#L127)                                           | Set to true to only present generated frames.                                                                             |
| [`generationRect`](../../framegeneration/include/ffx_framegeneration.h#L128)                                           | The area of the backbuffer that should be used for generation in case only a part of the screen is used e.g. due to movie bars                                                                             |
| [`frameID`](../../framegeneration/include/ffx_framegeneration.h#L129)                                           | Identifier used to select internal resources when async support is enabled. Must increment by exactly one (1) for each frame. Any non-exactly-one difference will reset the frame generation logic.                                                                             |


```C++
// Update frame generation config
FfxApiResource hudLessResource = SDKWrapper::ffxGetResourceApi(m_pHudLessTexture[m_curUiTextureIndex]->GetResource(), FFX_API_RESOURCE_STATE_COMPUTE_READ);

m_FrameGenerationConfig.frameGenerationEnabled = m_FrameInterpolation;
m_FrameGenerationConfig.flags                  = 0;
m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugTearLines ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES : 0;
m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugResetIndicators ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS : 0;
m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugView ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW : 0;
m_FrameGenerationConfig.HUDLessColor = (s_uiRenderMode == 3) ? hudLessResource : FfxApiResource({});
m_FrameGenerationConfig.allowAsyncWorkloads = m_AllowAsyncCompute && m_EnableAsyncCompute;
// assume symmetric letterbox
m_FrameGenerationConfig.generationRect.left   = (resInfo.DisplayWidth - resInfo.UpscaleWidth) / 2;
m_FrameGenerationConfig.generationRect.top    = (resInfo.DisplayHeight - resInfo.UpscaleHeight) / 2;
m_FrameGenerationConfig.generationRect.width  = resInfo.UpscaleWidth;
m_FrameGenerationConfig.generationRect.height = resInfo.UpscaleHeight;
// For sample purposes only. Most applications will use one or the other.
if (m_UseCallback)
{
    m_FrameGenerationConfig.frameGenerationCallback = [](ffxDispatchDescFrameGeneration* params, void* pUserCtx) -> ffxReturnCode_t
    {
        return ffxDispatch(reinterpret_cast<ffxContext*>(pUserCtx), &params->header);
    };
    m_FrameGenerationConfig.frameGenerationCallbackUserContext = &m_FrameGenContext;
}
else
{
    m_FrameGenerationConfig.frameGenerationCallback = nullptr;
    m_FrameGenerationConfig.frameGenerationCallbackUserContext = nullptr;
}
m_FrameGenerationConfig.onlyPresentGenerated = m_PresentInterpolatedOnly;
m_FrameGenerationConfig.frameID = m_FrameID;

m_FrameGenerationConfig.swapChain = GetSwapChain()->GetImpl()->DX12SwapChain();

ffx::ReturnCode retCode = ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig);
CauldronAssert(ASSERT_CRITICAL, !!retCode, L"Configuring FSR FG failed: %d", (uint32_t)retCode);
```

If using the frame generation callback, the swapchain will call the callback with appropriate parameters.
Otherwise, the application is responsible for calling the frame generation dispatch and setting parameters itself.
In that case, the frame ID must be equal to the frame ID used in configuration. The command list and output texture can be queried from the frame generation context using [`ffxQuery`](../../api/include/ffx_api.h#L160). See the [sample code](../../../../Samples/Upscalers/FidelityFX_FSR/dx12/fsrapirendermodule.cpp#L1172) for an example.

The user context pointers will only be passed into the respective callback functions. AMD FSR™ code will not attempt to dereference them.

When [`allowAsyncWorkloads`](../../framegeneration/include/ffx_framegeneration.h#L124) is set to `false` the main graphics queue will be used to execute the Optical Flow and Frame Generation workloads. It is strongly advised to profile, if significant performance benefits can be gained from asynchronous compute usage. Not using asynchronous compute will result in a lower memory overhead.

Note that UI composition and presents will always get executed on an async queue, so they can be paced and injected into the middle of the workloads generating the next frame.

![AMD FSR™ Frame Generation non async workflow](media/super-resolution-interpolation/02_FSR3_Flow.svg "A diagram showing the AMD FSR™ Frame Generation workflow when not using async compute")

When [`allowAsyncWorkloads`](../../framegeneration/include/ffx_framegeneration.h#L124) is set to `true`, the Optical Flow and Frame Generation workloads will run on an asynchronous compute queue and overlap with workloads of the next frame on the main game graphics queue. This can improve performance depending on the GPU and workloads.

![AMD FSR™ Frame Generation non async workflow](media/super-resolution-interpolation/03_FSR3_FlowAsyncQueue.svg "A diagram showing the AMD FSR™ Frame Generation workflow when using async compute")

<h3>Distortion Field</h3>

When an application applies post-processing distortion effects to the color buffer, the distorted color data no longer aligns spatially with the corresponding motion vectors and depth data. This would result in "ghosting" artifacts in the interpolated frame.

To address this, the application can configure the frame generation context with [`distortionField`](../../framegeneration/include/ffx_framegeneration.h#L182) linked after [`ffxConfigureDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L115) structure and calling [`ffxConfigure`](../../api/include/ffx_api.h#L154). 

**Distortion Field Format:**
- **2-component (R,G) texture** containing UV offset data
- **Each pixel encodes**: `[UV after distortion] - [UV before distortion]`
- **Frame Generation shaders** use these offsets to correct depth and motion vector lookups

<h3>UI Composition</h3>

For AMD FSR™ Frame Generation the user interface will require some special treatment, otherwise very noticeable artifacts will be generated which can impact readability of the interface. 

To prohibit those artifacts frame-generation supports various options to handle the UI:

The preferred method is to use the [`presentCallback`](../../framegeneration/include/ffx_framegeneration.h#L119). The function provided in this parameter will get called once for every frame presented and allows the application to schedule the GPU workload required to render the UI. By using this function the application can reduce UI input latency and render effects that do not work well with frame generation (e.g. film grain).

The UI composition callback function will be called for every frame (real or generated) to allow rendering the UI separately for each presented frame, so the UI can get rendered at presentation rate to achieve smooth UI animations.

```C++
ffxReturnCode_t FSR3RenderModule::UiCompositionCallback(ffxCallbackDescFrameGenerationPresent* params, void* userCtx)
{
    ID3D12GraphicsCommandList2* pDxCmdList  = reinterpret_cast<ID3D12GraphicsCommandList2*>(params->commandList);
    ID3D12Resource*             pRtResource = reinterpret_cast<ID3D12Resource*>(params->outputSwapChainBuffer.resource);
    ID3D12Resource*             pBbResource = reinterpret_cast<ID3D12Resource*>(params->currentBackBuffer.resource);

    // Use pDxCmdList to copy pBbResource and render UI into the outputSwapChainBuffer.
    // The backbuffer is provided as SRV so postprocessing (e.g. adding a blur effect behind the UI) can easily be applied

    return FFX_API_RETURN_OK;
}
```

![AMD FSR™ Frame Generation non async workflow](media/super-resolution-interpolation/04_FSR3_UiCallback.svg "A diagram showing the AMD FSR™ Frame Generation workflow when using async compute")

If frame generation is disabled [`presentCallback`](../../framegeneration/include/ffx_framegeneration.h#L119) will still get called on present.
![AMD FSR™ Frame Generation non async workflow](media/super-resolution-interpolation/05_FSR3_NoFG_UiCallback.svg "A diagram showing the AMD FSR™ Frame Generation workflow when using async compute")

The second option to handle the UI is to render the UI into a dedicated surface that will be blended onto the interpolated and real backbuffer before present. Composition of this surface can be done automatically composed by the proxy swapchain or manually in the [`presentCallback`](../../framegeneration/include/ffx_framegeneration.h#L119). This method allows to present a UI unaffected by AMD FSR™ Frame Generation, however the UI will only be rendered at render rate. For applications with a largely static UI this might be a good solution without the additional overhead of rendering the UI at presentation rate.

![AMD FSR™ Frame Generation non async workflow](media/super-resolution-interpolation/06_FSR3_UiTex.svg "A diagram showing the AMD FSR™ Frame Generation workflow when using async compute")

If frame generation is disabled and the UI Texture is provided, UI composition will still get executed by the AMD FSR™ Frame Generation Swapchain.
![AMD FSR™ Frame Generation non async workflow](media/super-resolution-interpolation/07_FSR3_NoFG_UiTex.svg "A diagram showing the AMD FSR™ Frame Generation workflow when using async compute")

In that case the surface needs to be registered to the swap chain by calling [`ffxConfigure`](../../api/include/ffx_api.h#L154) with a [`ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12`](../../framegeneration/include/dx12/ffx_api_framegeneration_dx12.h#L64) structure.

Flags can be provided in [`ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12`](../../framegeneration/include/dx12/ffx_api_framegeneration_dx12.h#L64) to control the following:

| [`FfxApiUiCompositionFlags`](../../framegeneration/include/ffx_framegeneration.h#L67) member | Note                                                                                                                               |
|---------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------|
| [`FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_USE_PREMUL_ALPHA`](../../framegeneration/include/ffx_framegeneration.h#L68)                                           | A bit indicating that we use premultiplied alpha for UI composition.                                                                             |
| [`FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING`](../../framegeneration/include/ffx_framegeneration.h#L69)                                           | When set, the SDK manages the UI texture lifetime — the application can safely reuse the UI texture immediately after `Present()` returns. When not set, the application must keep the UI texture valid until async composition of the current frame is complete (see [Resource Lifetime](#resource-lifetime)). |

```C++
FfxResource uiColor = ffxGetResource(m_pUiTexture[m_curUiTextureIndex]->GetResource(), L"FSR3_UiTexture", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
ffx::ConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12 uiConfig{};
uiConfig.uiResource = uiColor;
uiConfig.flags      = m_DoublebufferInSwapchain ? FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING : 0;
ffx::Configure(m_SwapChainContext, uiConfig);
```

The final method to handle the UI is to provide a [`HUDLessColor`](../../framegeneration/include/ffx_framegeneration.h#L125) surface in the [`FfxFrameGenerationConfig`](../../framegeneration/include/ffx_framegeneration.h#L283). This surface will get used during AMD FSR™ Frame Generation to detect the UI and avoid distortion on UI elements. This method has been added for compatibility with engines that can not apply either of the other two options for UI rendering.

![AMD FSR™ Frame Generation non async workflow](media/super-resolution-interpolation/08_FSR3_HUDLess.svg "A diagram showing the AMD FSR™ Frame Generation workflow when using async compute")

<h4>Different HUDless Formats</h4>

An optional structure [`ffxCreateContextDescFrameGenerationHudless`](../../framegeneration/include/ffx_framegeneration.h#L190) can be linked to the [`pNext`](../../api/include/ffx_api.h#L56) of the [`ffxCreateContextDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L74) used at context-creation time to enable the application to use a different [`hudlessBackBufferFormat`](../../framegeneration/include/ffx_framegeneration.h#L193) (IE.RGBA8_UNORM) from [`backBufferFormat`](../../framegeneration/include/ffx_framegeneration.h#L80) (IE. BGRA8_UNORM).


<h3>Dispatch frame generation preparation</h3>

Since version 3.1.0, frame generation runs independently of AMD FSR™ Upscaling. To replace the resources previously shared with the upscaler, a new frame generation prepare pass is required.

After the call to [`ffxConfigure`](../../api/include/ffx_api.h#L154), fill out a [`ffxDispatchDescFrameGenerationPrepareV2`](../../framegeneration/include/ffx_framegeneration.h#L231) structure. Then provide the structure and frame generation context in a call to [`ffxDispatch`](../../api/include/ffx_api.h#L165).

| [`ffxDispatchDescFrameGenerationPrepareV2`](../../framegeneration/include/ffx_framegeneration.h#L231) member | Note                                                                                                                               |
|---------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------|
| [`frameID`](../../framegeneration/include/ffx_framegeneration.h#L234)                                           | Identifier used to select internal resources when async support is enabled. Must increment by exactly one (1) for each frame. Any non-exactly-one difference will reset the frame generation logic. Set the frameID to the same value as in the [`ffxConfigureDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L115) structure.                                                                             |
| [`flags`](../../framegeneration/include/ffx_framegeneration.h#L235)                                           | Zero or combination of values from [`FfxApiDispatchFramegenerationFlags`](../../framegeneration/include/ffx_framegeneration.h#L52).                                                                             |
| [`commandList`](../../framegeneration/include/ffx_framegeneration.h#L236)                                           | A command list to record frame generation commands into.                                                                             |
| [`renderSize`](../../framegeneration/include/ffx_framegeneration.h#L237)                                           | The dimensions used to render game content, dilatedDepth, dilatedMotionVectors are expected to be of ths size.                                                                             |
| [`jitterOffset`](../../framegeneration/include/ffx_framegeneration.h#L238)                                           | The subpixel jitter offset applied to the camera.                                                                             |
| [`motionVectorScale`](../../framegeneration/include/ffx_framegeneration.h#L239)                                           | The scale factor to apply to motion vectors.                                                                             |
| [`frameTimeDelta`](../../framegeneration/include/ffx_framegeneration.h#L241)                                           | Time elapsed in milliseconds since the last frame.                                                                            |
| [`reset`](../../framegeneration/include/ffx_framegeneration.h#L242)                                           | A boolean value which when set to true, indicates FrameGeneration will be called in reset mode.                                                                             |
| [`cameraNear`](../../framegeneration/include/ffx_framegeneration.h#L243)                                           | The distance to the near plane of the camera.                                                                             |
| [`cameraFar`](../../framegeneration/include/ffx_framegeneration.h#L244)                                           | The distance to the far plane of the camera. This is used only used in case of non infinite depth.                                                                             |
| [`cameraFovAngleVertical`](../../framegeneration/include/ffx_framegeneration.h#L245)                                           | The camera angle field of view in the vertical direction (expressed in radians).                                                                             |
| [`viewSpaceToMetersFactor`](../../framegeneration/include/ffx_framegeneration.h#L246)                                           | The scale factor to convert view space units to meters.                                                                             |
| [`depth`](../../framegeneration/include/ffx_framegeneration.h#L247)                                           | The depth buffer data.                                                                             |
| [`motionVectors`](../../framegeneration/include/ffx_framegeneration.h#L248)                                           | The motion vector data.                                                                             |
| [`cameraPosition`](../../framegeneration/include/ffx_framegeneration.h#L250)                                           | The camera position in world space.                                                                             |
| [`cameraUp`](../../framegeneration/include/ffx_framegeneration.h#L251)                                           | The camera up normalized vector in world space.                                                                             |
| [`cameraRight`](../../framegeneration/include/ffx_framegeneration.h#L252)                                           | The camera right normalized vector in world space.                                                                             |
| [`cameraForward`](../../framegeneration/include/ffx_framegeneration.h#L253)                                           | The camera forward normalized vector in world space.                                                                             |

For fields also found in [`ffxDispatchDescUpscale`](../../upscalers/include/ffx_upscale.h#L90), the same input requirements and recommendations apply here.

Set the frameID to the same value as in the configure description.

It is required to specify [`cameraPosition`](../../framegeneration/include/ffx_framegeneration.h#L250), [`cameraUp`](../../framegeneration/include/ffx_framegeneration.h#L251), [`cameraRight`](../../framegeneration/include/ffx_framegeneration.h#L252) and [`cameraForward`](../../framegeneration/include/ffx_framegeneration.h#L253) which must contain valid information about the camera position and orientation within the scene.

<h3>Dispatch frame generation</h3>

In order to optimally invoke [`ffxDispatch`](../../api/include/ffx_api.h#L165) for frame-generation it is *strongly* recommended that the caller supply a callback to [`frameGenerationCallback`](../../framegeneration/include/ffx_framegeneration.h#L121) along with any necessary context pointer in [`frameGenerationCallbackUserContext`](../../framegeneration/include/ffx_framegeneration.h#L122). This callback should execute [`ffxDispatch`](../../api/include/ffx_api.h#L165) using the provided [`ffxDispatchDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L97) structure pointer. This will then be written into the proper command-list during the swap-chain presentation and when [`allowAsyncWorkloads`](../../framegeneration/include/ffx_framegeneration.h#L124) are enabled overlapped with other work.

It is possible, but *highly* discouraged to dispatch frame-generation manually. This infrastructure exists to support specific game-engine integrations such as the Unreal Engine plugin where dispatching during swap-chain presentation is unsafe. In this case the caller must acquire an interpolation command-buffer using the [`ffxQueryDescFrameGenerationSwapChainInterpolationCommandListDX12`](../../framegeneration/include/dx12/ffx_api_framegeneration_dx12.h#L72) structure on the swap-chain context and supply this as the [`commandList`](../../framegeneration/include/ffx_framegeneration.h#L100) in a [`ffxDispatchDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L97) which can then be used to dispatch via [`ffxDispatch`](../../api/include/ffx_api.h#L165). In this case the caller is responsible for understanding whether it is safe to enable [`allowAsyncWorkloads`](../../framegeneration/include/ffx_framegeneration.h#L124).

| [`ffxDispatchDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L97) member | Note                                                                                                                               |
|---------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------|
| [`commandList`](../../framegeneration/include/ffx_framegeneration.h#L100)                                           | The command list on which to register render commands.                                                                             |
| [`presentColor`](../../framegeneration/include/ffx_framegeneration.h#L101)                                           | The current presentation color, this will be used as source data.                                                                            |
| [`outputs`](../../framegeneration/include/ffx_framegeneration.h#L102)                                           | Destination targets (1 for each frame in numGeneratedFrames).                                                                             |
| [`numGeneratedFrames`](../../framegeneration/include/ffx_framegeneration.h#L103)                                           | The number of frames to generate from the passed in color target.                                                                            |
| [`reset`](../../framegeneration/include/ffx_framegeneration.h#L104)                                           | A boolean value which when set to true, indicates the camera has moved discontinuously.                                                                            |
| [`backbufferTransferFunction`](../../framegeneration/include/ffx_framegeneration.h#L105)                                           | The transfer function use to convert frame generation source color data to linear RGB. One of the values from [`FfxApiBackbufferTransferFunction`](../../api/include/ffx_api_types.h#L132).                                                                             |
| [`minMaxLuminance`](../../framegeneration/include/ffx_framegeneration.h#L106)                                           | Min and max luminance values, used when converting HDR colors to linear RGB.                                                                             |
| [`generationRect`](../../framegeneration/include/ffx_framegeneration.h#L107)                                           | The area of the backbuffer that should be used for generation in case only a part of the screen is used e.g. due to movie bars.                                                                             |
| [`frameID`](../../framegeneration/include/ffx_framegeneration.h#L108)                                           | Identifier used to select internal resources when async support is enabled. Must increment by exactly one (1) for each frame. Any non-exactly-one difference will reset the frame generation logic.                                                                             |

<h3>Input color requirements</h3>

Input color dimension to AMD FSR™ Frame Generation needs to be [`displaySize`](../../framegeneration/include/ffx_framegeneration.h#L78). Recall that input color comes from swapchain back buffer if using [`frameGenerationCallback`](../../framegeneration/include/ffx_framegeneration.h#L121) or [`presentColor`](../../framegeneration/include/ffx_framegeneration.h#L101) if manually dispatch frame generation.

If the upscaled (or rendered, if no upscaling) image is not same dimension as `displaySize`, it must be composited before passing as input color to AMD FSR™ Frame Generation. 
 
The upscaled image composited in the back buffer must have the same aspect ratio as [`renderSize`](../../framegeneration/include/ffx_framegeneration.h#L237). 

<h3>Letterboxing or pillarboxing</h3>

Recall that the interpolated pixel color comes from lookup where the interpolated pixel would be in the previous and current back buffers using motion vector. When an application presents an image with cinematic bars (i.e. letterboxing or pillarboxing), the lookup pixel position in the back buffer can be inside the black bars region resulting in image artifacts in the interpolated image. IE. color region spill into the black bar region, or black lines appear inside the color region. To fix this, the application needs to fill out [`generationRect`](../../framegeneration/include/ffx_framegeneration.h#L128), to tell AMD FSR™ Frame Generation, the area of back buffer containing rendered content without black bars. At each frame, the `left` and `top` values can vary. But, current API does not support `width` and `height` values different than the upscaler output size. AMD FSR™ Frame Generation assumes the top-left pixel of `generationRect` maps to the top-left texel in [`depth`](../../framegeneration/include/ffx_framegeneration.h#L247) and [`motionVectors`](../../framegeneration/include/ffx_framegeneration.h#L248). The current API does not describe how to offset into the depth and motion vector textures when `generationRect` size is smaller than upscaler output size.

> **Example supported usage**  
Suppose upscaler output is 3840x2160 (16:9) without black bars and is displayed on 3840x2160 (16:9) sized back buffer. Then, it is expected to set `generationRect` to cover the 3840x2160 region.

> **Example supported usage**  
> Suppose upscaler output is 3840x1080 (32:9) without black bars. Upscaler output is composed to a 3840x2160 (16:9) sized back buffer and centered vertically, leaving black bars at the top and bottom. Then it is correct to set `generationRect` to only cover the 3840x1080 region. AMD FSR™ Frame Generation has correct mapping where top-left pixel of `generationRect` with UV coordinate (0, 0.25) in backbuffer maps to UV coordinate (0, 0) in the [`depth`](../../framegeneration/include/ffx_framegeneration.h#L247) and [`motionVectors`](../../framegeneration/include/ffx_framegeneration.h#L248). 

> **Example unsupported usage**  
> Suppose upscaler output is 3840x2160 (16:9) without black bar. Black bar is drawn over the upscaler output color such that the 3840x2160 (16:9) sized back buffer has now a 32:9 image (3840x1080) centered vertically, with black bars at the top and bottom. If set `generationRect` to be 3840x1080 colored region, then the assumed mapping of `generationRect` to depth and motion vector is wrong for every pixel, resulting in "ghosting" artifacts in the non-black region. If set `generationRect` to be 3840x2160, then the interpolated frame will have image artifacts due to lookup inside the black region.

<h3>Shutdown</h3>

Before destroying the Frame Generation context:
- Disable frame generation and UI composition on the proxy swap chain via `ffx::Configure`.
- This configure call waits for the proxy swap chain to complete GPU work (interpolation and UI composition) that references Frame Generation resources.
- Then destroy the Frame Generation context with [`ffxDestroyContext`](../../api/include/ffx_api.h#L148) to release all resources it created.

```C++
if (m_FrameGenContext != nullptr)
{
    // Disable frame generation before destroying context.
    // Internally calls waitForPresents(), which flushes interpolation and UI composition GPU work, to avoid
    // D3D12 debug layer error #921: OBJECT_DELETED_WHILE_STILL_IN_USE
    ffx::ConfigureDescFrameGeneration m_FrameGenerationConfig;
    m_FrameGenerationConfig.frameGenerationEnabled = false;
    m_FrameGenerationConfig.swapChain              = GetSwapChain()->GetImpl()->DX12SwapChain();
    m_FrameGenerationConfig.presentCallback        = nullptr;
    m_FrameGenerationConfig.HUDLessColor           = FfxApiResource({});
    ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig);

    ffx::DestroyContext(m_FrameGenContext);
    m_FrameGenContext = nullptr;
}
```

Failure to flush the interpolation GPU work from submitted frames  can result in D3D12 debug layer error EXECUTION ERROR #921: OBJECT_DELETED_WHILE_STILL_IN_USE on frame generation created resources during callstack of `ffxDestroyContext` on the frame generation context. 

<h3>Thread safety</h3>

The underlying context is not guaranteed to be thread safe. Several public APIs that operate on `FrameGenContext` and `SwapChainContext` are not thread-safe and must be externally synchronized. Race condition symptoms can include access violation crashes, interpolation visual artifacts, and infinite waits in `Dx12CommandPool` destructor when releasing the swapchain.

When `AMD FSR™ Frame Generation Swapchain` `::present` is configured with frame generation callback, it accesses both `SwapChainContext` and `FrameGenContext`. A race can occur if application threads call `::present` concurrently with either `Dispatch(m_FrameGenContext, DispatchDescFrameGenerationPrepareV2)` or `DestroyContext(SwapChainContext)`.

APIs that require a mutex guard
- [`ffxCreateContext`](../../api/include/ffx_api.h#L142) for the Frame Generation context
- [`ffxDestroyContext`](../../api/include/ffx_api.h#L148) for the Frame Generation context
- [`ffxDispatch`](../../api/include/ffx_api.h#L165) [`ffxDispatchDescFrameGenerationPrepareV2`](../../framegeneration/include/ffx_framegeneration.h#L231) structure
- [`ffxDispatch`](../../api/include/ffx_api.h#L165) [`ffxDispatchDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L97) structure if `AMD FSR™ Frame Generation Swapchain` `::present` is not configured with [`frameGenerationCallback`](../../framegeneration/include/ffx_framegeneration.h#L121).
- `AMD FSR™ Frame Generation Swapchain` `::present` if app supplied a valid [`frameGenerationCallback`](../../framegeneration/include/ffx_framegeneration.h#L121) within [`ffxConfigureDescFrameGeneration`](../../framegeneration/include/ffx_framegeneration.h#L115) structure.
- `AMD FSR™ Frame Generation Swapchain` swapchain destructor. See [`Shutdown`](../techniques/frame-interpolation-swap-chain.md#Shutdown).


```C++
std::mutex g_FGContextMutex;

struct FGContextScopeLock
{
    explicit FGContextScopeLock(bool doLock) : m_locked(doLock)
    {
        if (m_locked) g_FGContextMutex.lock();
    }
    ~FGContextScopeLock()
    {
        if (m_locked) g_FGContextMutex.unlock();
    }
private:
    bool m_locked;
};

ffx::Context m_FrameGenContext  = nullptr;
ffx::Context m_SwapChainContext = nullptr;
ffx::ConfigureDescFrameGeneration m_FrameGenerationConfig{};

{
    // Acquire lock before creating FG context
    cauldron::FGContextScopeLock lock(true);
    ffx::CreateContextDescFrameGeneration createFg{};
    ffx::CreateBackendDX12Desc backendDesc{};
    ffx::CreateContextDescFrameGenerationVersion headerVersion{};
    headerVersion.version = FFX_FRAMEGENERATION_VERSION;
    ffx::CreateContext(m_FrameGenContext, nullptr, createFg, backendDesc, headerVersion);
}
{
    // Acquire lock before dispatch FG Prepare
    cauldron::FGContextScopeLock lock(true);
    ffx::DispatchDescFrameGenerationPrepareV2 dispatchFgPrep{};
    ffx::Dispatch(m_FrameGenContext, dispatchFgPrep);
}
{
    // Acquire lock if app dispatch frame generation directly
    cauldron::FGContextScopeLock lock(true);
    ffx::DispatchDescFrameGeneration dispatchFg{};
    ffx::Dispatch(m_FrameGenContext, dispatchFg);
}
{
    // Acquire lock only if the proxy swapchain present operation uses the FG callback to dispatch frame generation.
    const bool bPresentWillDispatchFG = cauldron::GetFramework()->IsFrameGenerationSwapchain() && cauldron::GetFramework()->FrameInterpolationEnabled() && cauldron::GetFramework()->IsSwapchainUsingFrameGenerationCallback();
    cauldron::FGContextScopeLock fgLock(bPresentWillDispatchFG);
    pSwapChain->GetImpl()->m_pSwapChain->Present(1, 0);
}
{
    // Acquire lock before destroying FG context
    cauldron::FGContextScopeLock lock(true);
    m_FrameGenerationConfig.frameGenerationEnabled = false;
    ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig);
    ffx::DestroyContext(m_FrameGenContext);
}
{
    // Acquire lock before destroying proxy swapchain
    cauldron::FGContextScopeLock lock(true);
    IDXGISwapChain4*     pSwapChain4 = pSwapchain->GetImpl()->DX12SwapChain();
    // Release final reference triggers proxy swap chain destructor when refCount reaches 0.
    ULONG refCount = pSwapChain4->Release();
}

```
<h3>Resource Lifetime</h3>

<h4>When UiTexture composition mode is used</h4> 

<h5>If FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING is set:</h5>

The SDK takes ownership of the [`currentUI`](../../framegeneration/include/ffx_framegeneration.h#L90) texture at `Present()` time.
The application can safely reuse the UI texture **immediately after `Present()` returns** — no additional synchronization is required.

<h5>If FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING is not set:</h5>

The [`currentUI`](../../framegeneration/include/ffx_framegeneration.h#L90) texture remains in use by the SDK after `Present()` returns, until async composition of the current frame is complete.
This typically overlaps with the start of the next frame, so the application must not modify the UI texture during this period. The application must double-buffer the UI texture itself.

<h4>When HUDLess composition mode is used:</h4>

The HUDLess texture will be used during frame interpolation.
The application is responsible for ensuring it persists until frame interpolation is complete.
If [`allowAsyncWorkloads`](../../framegeneration/include/ffx_framegeneration.h#L124) is true:
Frame interpolation happens on an async compute queue, so the HUDLess texture needs to be double buffered by the application.
If [`allowAsyncWorkloads`](../../framegeneration/include/ffx_framegeneration.h#L124) is false:
Frame interpolation happens on the game GFX queue, so the app can safely modify the HUDLess texture in the next frame.

<h4>When distortionField texture is registered to frame interpolation:</h4>

The application is responsible for ensuring the [`distortionField`](../../framegeneration/include/ffx_framegeneration.h#L185) texture persists until frame interpolation is complete.
If [`allowAsyncWorkloads`](../../framegeneration/include/ffx_framegeneration.h#L124) is true:
Frame interpolation happens on an async compute queue, so the [`distortionField`](../../framegeneration/include/ffx_framegeneration.h#L185) texture needs to be double buffered by the application.
If [`allowAsyncWorkloads`](../../framegeneration/include/ffx_framegeneration.h#L124) is false:
Frame interpolation happens on the game GFX queue, so the app can safely modify the [`distortionField`](../../framegeneration/include/ffx_framegeneration.h#L185) texture in the next frame.

<h3>Debug Checker</h3>
Enable debug checker to validate application supplied inputs at dispatch upscale. It is recommended this is enabled only in development builds of game.

Passing 
[`FFX_FRAMEGENERATION_ENABLE_DEBUG_CHECKING`](../../framegeneration/include/ffx_framegeneration.h#L46) flag within [`FfxApiCreateContextFramegenerationFlags`](../../framegeneration/include/ffx_framegeneration.h#L38) will output textual warnings from frame generation to debugger TTY by default. Calling [`ffxConfigure`](../../api/include/ffx_api.h#L154) with [`fpMessage`](../../api/include/ffx_api.h#L84) within a [`ffxConfigureDescGlobalDebug1`](../../api/include/ffx_api.h#L81) structure to a suitable function allows the application to receive the debug messages issued. 

An example of the kind of output that can occur when debug checker observes possible issues is below:

FSR_API_DEBUG_WARNING: ffxDispatchDescFrameGenerationPrepareCameraInfo needs to be passed as linked struct. This is a required input to FSR3.1.4 and onwards for best quality.

<h3>Debug output</h3>

The AMD FSR™ Frame Generation API supports several debug visualisation options:

When [`FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES`](../../framegeneration/include/ffx_framegeneration.h#L54) is set in the flags attribute of [`ffxDispatchDescFrameGenerationPrepareV2`](../../framegeneration/include/ffx_framegeneration.h#L235), the inpainting pass will add bars of changing color on the left and right border of the interpolated image. This will assist visualizing if interpolated frames are getting presented and if the frames are presented with tearing enabled.

When [`FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS`](../../framegeneration/include/ffx_framegeneration.h#L55) is set in the flags attribute of [`ffxDispatchDescFrameGenerationPrepareV2`](../../framegeneration/include/ffx_framegeneration.h#L235), the debug reset indicators will be drawn to the generated output.

When [`FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW`](../../framegeneration/include/ffx_framegeneration.h#L56) is set in the flags attribute of [`ffxDispatchDescFrameGenerationPrepareV2`](../../framegeneration/include/ffx_framegeneration.h#L235), the [FrameInterpolationSwapChain](frame-interpolation-swap-chain.md) will only present interpolated frames and execute an additional pass to render debug data from internal surfaces onto the interpolated frame, to allow you to debug.

When [`FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_PACING_LINES`](../../framegeneration/include/ffx_framegeneration.h#L58) is set in the flags attribute of [`ffxDispatchDescFrameGenerationPrepareV2`](../../framegeneration/include/ffx_framegeneration.h#L235), the debug pacing lines will be drawn to the generated output.

![AMD FSR™ Frame Generation debug overlay](media/frame-interpolation/frame-interpolation-debug-overlay.svg "A diagram showing the debug overlay")

<h2>See also</h2>

- [AMD FSR™ Frame Generation 4.0.1](frame-interpolation-ml.md)
- [AMD FidelityFX™ Super Resolution Frame Generation 3.1.6](frame-interpolation.md)
- [AMD FSR™ Upscaling and Frame Generation Sample](../../../../docs/samples/super-resolution.md)
- [AMD FSR™ Naming guidelines](../getting-started/naming-guidelines.md)
