# Skin controls and Onimusha compatibility changes

## Automatic mask versus skin colour controls

`AutoMask` is NVIDIA's internal automatic-mask input. It was already sent during feature creation
and evaluation, and changing an active pass's setting already rebuilt that feature. `SkinStructure=-1`
follows `LocalStructure`, so the default does not request separate skin detail strength. To compare,
try `AutoMask=true`, `LocalStructure=1`, `SkinStructure=0` on a still character close-up.
`SkinStructure=0` is not a switch that removes all skin lighting/colour changes.

The forwarder now explicitly clears `DLSSNR.ControlMask` before setting AutoMask. An explicit mask
overrides the automatic mask; a stale entry in the reusable parameter block must not win silently.
This is defensive handling, not confirmation that stale masks caused a particular user's report.
The ControlMask precedence is also described in [these independent feature-18 probes](https://github.com/kibblerz/DLSS5-Reshade-AIO/blob/main/lab/PRIVATE-CONTRACT-FINDINGS.md#controlmask).
Logs distinguish parameter-table readback from proof of a visible effect and identify the final
model pass whose values remain in that table.

The new **Skin and environment (final edit)** section is independent of AutoMask. It has:

- An opt-in **Separate skin / environment controls** switch.
- **Allow skin tone / colour changes**, a separate on/off switch.
- Skin and environment sliders for detail/lighting and colour (0..1).
- A preview showing the colour-based selection in white.

This is an approximate colour-based selection, not a semantic skin/face detector. Wood, sand and
other warm materials can match; unusual or strongly coloured lighting can prevent skin from matching.
Check the preview before using it. The filter is off by default and all four strengths default to 1,
so existing setups keep their output. It applies once to the combined NR result, including replace
mode, on DX12 and native Vulkan. It does not add another neural model pass. Per-pass model sliders
remain separate; the final filter does not expose the model's private semantic mask.

Colour off preserves hue/chroma in fully selected pixels but allows the lighting slider to change
brightness. Set skin detail/lighting to 0 as well to restore original pixels where the mask is fully
white. At soft edges the skin and environment settings blend.

## Onimusha: Way of the Sword

Upstream's [issue #22](https://github.com/Dagherbou/OptiScaler_DLSSNR/issues/22) reports NR crashes on
multiple GPU generations, including during loading. This fork contained the RE Engine compute-state
quirk for `onimushawots_demo.exe`, but not `onimushawots.exe`. Both now get the same restore/spoofing
defaults. An explicit `RestoreComputeSignature=false` in an old INI overrides the automatic fix;
use `auto` or `true` under `[Hotfix]`.

NR's graphics-state envelope previously started after model creation and some early returns. It now
covers creation, recreation and evaluation. When the required game state cannot be restored, the
frame is skipped before any NR command recording. These changes target concrete code gaps, but the
retail game/RTX 30 combination has not been reproduced here, so this is a candidate fix.
The DX12 forwarder also rejects a handle if model creation returned an error, matching its DX11/Vulkan
paths, rather than treating a partially created feature as usable.

Testing order for an affected machine:

1. Confirm the active proxy DLL was replaced, not just the unused `OptiScaler.dll` in the folder.
2. Use the documented RTX 20/30/40 NR runtime, one pass, and disable FG for the first test.
3. Try DLSS + NR in gameplay and through loading/fast travel. Attach the new log if it still crashes.
4. Test FSR output separately to isolate the native DLSS path. Enabling DLSS as the game's input
   and choosing FSR as OptiScaler's output are different settings; state both in a report.
5. On RTX 30, use a supported replacement FG provider if needed, not the RTX 40 MFG unlocker.

Also report the driver, DLSS DLL version, NR runtime hash, real executable name and INI. Do not assume
an out-of-memory, resource-lifetime or other loading bug is fixed just because the engine quirk is on.

## Validation

The Release x64 OptiScaler DLL and forwarder build successfully. Both DX12 DXIL and Vulkan SPIR-V
shaders compile. `tests/nr_skin_shader_smoke.cpp` executes the shared HLSL headlessly using D3D11 WARP:
disabled/default identity, full bypass, skin/environment separation, mask preview and colour preservation.
This is shader validation, not an in-game test of NVIDIA's automatic mask.

Build/run from a VS x64 Native Tools prompt at the repository root:

```bat
cl /nologo /std:c++20 /EHsc tests\nr_skin_shader_smoke.cpp /Fe:x64\nr_skin_shader_smoke.exe /Fo:x64\nr_skin_shader_smoke.obj /link d3d11.lib d3dcompiler.lib
x64\nr_skin_shader_smoke.exe OptiScaler\shaders\dlssnr\precompile\dlssnr.hlsl
```

RTX 40 unlocker setup and its separate source-build instructions are in [RTX40-MFG.md](RTX40-MFG.md).
