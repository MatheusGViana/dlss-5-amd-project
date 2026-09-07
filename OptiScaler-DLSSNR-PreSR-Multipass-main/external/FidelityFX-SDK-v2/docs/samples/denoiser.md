<!-- @page page_samples_denoiser AMD FSR™ Ray Regeneration -->

# AMD FSR™ Ray Regeneration Sample

![alt text](media/denoiser/fsr-ray-regeneration-sample.png "A screenshot of the AMD FSR™ Ray Regeneration sample.")

This [sample](../../Samples/Denoisers/FidelityFX_Denoiser/dx12/FidelityFX_Denoiser_Sample_2022.sln) demonstrates how to integrate and experiment with AMD FSR™ Ray Regeneration 1.2.0, a real-time ray-tracing denoising solution designed to improve the stability and visual quality of ray-traced lighting signals.

For details on the underlying algorithm, refer to the [AMD FSR™ Ray Regeneration](../../Kits/FidelityFX/docs/techniques/denoising.md) technique documentation.

## Table of contents

- [1. Requirements](#1-requirements)
- [2. UI elements](#2-ui-elements)
  - [2.1. Provider](#21-provider)
  - [2.2. Context creation](#22-context-creation)
  - [2.3. Configure](#23-configure)
  - [2.4. Dispatch](#24-dispatch)
  - [2.5. Display](#25-display)
- [3. Setting up AMD FSR™ Ray Regeneration](#3-setting-up-amd-fsr-ray-regeneration)
- [4. Sample controls and configurations](#4-sample-controls-and-configurations)
- [5. See also](#5-see-also)

## 1. Requirements

- AMD FSR™ Ray Regeneration requires an AMD Radeon™ RX 9000 Series GPU or later.
- DirectX® 12 + Shader Model 6.6
- Windows® 11

> [!NOTE]
> AMD FSR™ Ray Regeneration is only supported on AMD Radeon™ RX 9000 Series GPUs or newer.
> On unsupported GPUs, the sample will still run, but denoising features will be disabled.

## 2. UI elements

The sample contains various UI elements to help you explore the techniques it demonstrates.
The tables below summarize the UI elements and what they control within the sample.

### 2.1. Provider

| **Element name** | **Values** | **Description** |
| -----------------|------------|-----------------|
| **Version** | `AMD FSR™ Ray Regeneration - 1.2.0` | Dropdown for specifying the provider.<br><br>Sets the corresponding [`versionId`](../../Kits/FidelityFX/api/include/ffx_api.h#L112) member of the [`ffxOverrideVersion`](../../Kits/FidelityFX/api/include/ffx_api.h#L109) description. |

### 2.2. Context creation

| **Element name** | **Values** | **Description** |
| -----------------|------------|-----------------|
| **Enable direct diffuse denoising** | Checked,<br>Unchecked<br> | Checkbox for specifying whether the direct diffuse radiance should be denoised.<br><br>Toggles the [`FFX_DENOISER_SIGNAL_DIRECT_DIFFUSE`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L561) flag to the [`signalFlags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L96) member of the [`ffxCreateContextDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L82) context creation description. |
| **Enable direct specular denoising** | Checked,<br>Unchecked<br> | Checkbox for specifying whether the direct specular radiance should be denoised.<br><br>Toggles the [`FFX_DENOISER_SIGNAL_DIRECT_SPECULAR`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L563) flag to the [`signalFlags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L96) member of the [`ffxCreateContextDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L82) context creation description. |
| **Enable indirect diffuse denoising** | Checked,<br>Unchecked<br> | Checkbox for specifying whether the indirect diffuse radiance should be denoised.<br><br>Toggles the [`FFX_DENOISER_SIGNAL_INDIRECT_DIFFUSE`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L567) flag to the [`signalFlags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L96) member of the [`ffxCreateContextDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L82) context creation description. |
| **Enable indirect specular denoising** | Checked,<br>Unchecked<br> | Checkbox for specifying whether the indirect specular radiance should be denoised.<br><br>Toggles the [`FFX_DENOISER_SIGNAL_INDIRECT_SPECULAR`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L569) flag to the [`signalFlags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L96) member of the [`ffxCreateContextDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L82) context creation description. |
| **Enable dominant light visibility denoising** | Checked,<br>Unchecked<br> | Checkbox for specifying whether the dominant light visibility should be denoised.<br><br>Toggles the [`FFX_DENOISER_SIGNAL_DOMINANT_LIGHT_VISIBILITY`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L565) flag to the [`signalFlags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L96) member of the [`ffxCreateContextDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L82) context creation description. |
| **Enable ambient occlusion denoising** | Checked,<br>Unchecked<br> | Checkbox for specifying whether the ambient occlusion should be denoised.<br><br>Toggles the [`FFX_DENOISER_SIGNAL_AMBIENT_OCCLUSION`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L559) flag to the [`signalFlags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L96) member of the [`ffxCreateContextDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L82) context creation description. |
| **Enable specular occlusion denoising** | Checked,<br>Unchecked<br> | Checkbox for specifying whether the specular occlusion should be denoised.<br><br>Toggles the [`FFX_DENOISER_SIGNAL_SPECULAR_OCCLUSION`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L571) flag to the [`signalFlags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L96) member of the [`ffxCreateContextDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L82) context creation description. |
| **Enable debugging** | Checked,<br>Unchecked<br> | Checkbox for specifying whether debugging should be enabled.<br><br>Toggles the [`FFX_DENOISER_ENABLE_DEBUGGING`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L517) flag to the [`flags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L104) member of the [`ffxCreateContextDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L82) context creation description. |
| **Enable validation** | Checked,<br>Unchecked<br> | Checkbox for specifying whether exhaustive validation should be enabled.<br><br>Toggles the [`FFX_DENOISER_ENABLE_VALIDATION`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L519) flag to the [`flags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L104) member of the [`ffxCreateContextDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L82) context creation description. |

### 2.3. Configure

| **Element name** | **Values** | **Description** |
| -----------------|------------|-----------------|
| **Cross bilateral normal strength** | [0.0, 1.0] | Sets the [`FFX_API_CONFIGURE_DENOISER_KEY_CROSS_BILATERAL_NORMAL_STRENGTH`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L526) configuration parameter. |
| **Disocclusion threshold** | [0.01, 0.05] | Sets the [`FFX_API_CONFIGURE_DENOISER_KEY_DISOCCLUSION_THRESHOLD`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L541) configuration parameter. |
| **Gaussian kernel relaxation** | [0.0, 1.0] | Sets the [`FFX_API_CONFIGURE_DENOISER_KEY_GAUSSIAN_KERNEL_RELAXATION`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L538) configuration parameter. |
| **Max radiance** | [0.0, 65504.0] | Sets the [`FFX_API_CONFIGURE_DENOISER_KEY_MAX_RADIANCE`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L532) configuration parameter. |
| **Radiance std clip** | [0.0, 65504.0] | Sets the [`FFX_API_CONFIGURE_DENOISER_KEY_RADIANCE_CLIP_STD_K`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L535) configuration parameter. |
| **Stability bias** | [0.0, 1.0] | Sets the [`FFX_API_CONFIGURE_DENOISER_KEY_STABILITY_BIAS`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L529) configuration parameter. |
| **Debug view linear depth max bound** | [0.001, 1024.0] | Sets the `max` field of the [`FFX_API_CONFIGURE_DENOISER_KEY_DEBUG_VIEW_LINEAR_DEPTH_BOUNDS`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L544) configuration parameter. Only the max bound is exposed in the sample UI; the min bound is set programmatically. |

### 2.4. Dispatch

| **Element name** | **Values** | **Description** |
| -----------------|------------|-----------------|
| **Reset** | NA | Button for resetting the history accumulation.<br><br>Toggles the [`FFX_DENOISER_DISPATCH_RESET`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L577) flag to the [`flags`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L281) member of the [`ffxDispatchDescDenoiser`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L167) dispatch description. |

### 2.5. Display

| **Element name** | **Values** | **Description** |
| -----------------|------------|-----------------|
| **View mode** | Default,<br>Default (Input),<br>Direct,<br>Direct diffuse,<br>Direct specular,<br>Indirect,<br>Indirect diffuse,<br>Indirect specular,<br>Dominant light visibility,<br>Ambient occlusion,<br>Specular occlusion,<br>Direct (Input),<br>Direct diffuse (Input),<br>Direct specular (Input),<br>Indirect (Input),<br>Indirect diffuse (Input),<br>Indirect specular (Input),<br>Dominant light visibility (Input),<br>Ambient occlusion (Input),<br>Specular occlusion (Input),<br>Linear depth,<br>Motion vectors,<br>Normals,<br>Specular albedo,<br>Diffuse albedo,<br>Skip signal | Dropdown for specifying the view mode to display. |
| ${\color{red}R}$ ${\color{green}G}$ ${\color{blue}B}$ ${\color{white}A}$ | Checked,<br>Unchecked<br> | Checkboxes for specifying which color channels (i.e. ${\color{red}red}$, ${\color{green}green}$, ${\color{blue}blue}$, ${\color{white}alpha}$) of the selected view mode should be displayed. |
| **Show debug view** | Checked,<br>Unchecked<br> | Checkbox for specifying whether the debug view should be dispatched and drawn. The debug view requires debugging to be enabled.
| **Debug view mode** | Overview,<br>Fullscreen Target<br> | Dropdown for specifying the debug view mode to display.<br><br>Sets the corresponding [`mode`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L308) member of the [`ffxDispatchDescDenoiserDebugView`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L291) dispatch description.
| **Debug viewport index** | [0,`FFX_API_DENOISER_DEBUG_VIEW_MAX_VIEWPORTS`) | Slider for specifying the debug viewport index for the Fullscreen Target, debug view mode.<br><br>Sets the corresponding [`viewportIndex`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L312) member of the [`ffxDispatchDescDenoiserDebugView`](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h#L291) dispatch description.


## 3. Setting up AMD FSR™ Ray Regeneration

The sample includes a [dedicated Render Module for AMD FSR™ Ray Regeneration](../../Samples/Denoisers/FidelityFX_Denoiser/dx12/denoiserrendermodule.h) which creates the context and manages its lifetime. See the [AMD FSR™ Ray Regeneration](../../Kits/FidelityFX/docs/techniques/denoising.md) technique documentation for more details.

## 4. Sample controls and configurations

For information on sample controls, configuration options, and Cauldron Framework UI elements, see [Running the samples](../getting-started/running-samples.md).

## 5. See also

- [AMD FSR™ Ray Regeneration API](../../Kits/FidelityFX/denoisers/include/ffx_denoiser.h)
- [AMD FSR™ Ray Regeneration technique documentation](../../Kits/FidelityFX/docs/techniques/denoising.md)