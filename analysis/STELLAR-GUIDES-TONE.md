# Stellar Blade test build: tone and input guides

2026-09-07. Changes follow the user's confirmation that the adapter-spoof and FFX colour-barrier fixes allowed the neural pass to complete.

## Observed evidence

- The game's log rejected DLSS input with active colour 1719x720 and display motion 2580x1080, createFlags=9.
- FFX input completed thousands of neural frames, but screenshots with Tone around 0.99 were substantially darker than neural-disabled screenshots.
- The pinned original AMD binary contains this UI text (file offset 0x6542a; analysis/version-strings.txt): "Broad lighting and colour response. This DLL build gates these channels off; raising it mostly darkens the frame."
- The runtime accepts exposure only as R32_FLOAT. The previous bridge discarded exposure for all games.
- Streamline announced multiframe support and loaded DLSS Enabler. The available logs do not establish successful FG evaluation.

## Changes

- AMD bridge passes Tone=0 regardless of saved NVIDIA/shared tone settings. AMD panel explains that tone is unavailable in this runtime. Structure and skin remain adjustable. This does not restore the missing lighting/colour channels in the closed binary.
- Display-resolution motion is sampled at render-pixel centres into an FP16 RG texture; packet motion scales are multiplied by render/display extent. The original game motion texture and its parameters remain unchanged for SR and FG.
- Supported float exposure textures are converted from their first channel into a 1x1 R32_FLOAT texture, accounting for exposureScale/preExposure. Unsupported layouts keep the existing automatic-exposure fallback.
- Changed guide mappings invalidate neural history. Scratch resources and descriptors use the existing submission/fence lifetime protection.
- Added transition-based logging for Streamline DLSSG options and replacement evaluation results. No forced FG enable or invented frame counts.

## Validation

- Full Release x64 build: analysis/build-stellar-guides-tone.log (exit 0; linker warnings remain).
- analysis/smoke-guide-conversion.txt: 8 GPU frames; verified native exposure 2 from float4 input 4 and preExposure 2; native motion scale 128 from a 256-wide source; no nonfinite pixels or reported D3D12 errors.
- analysis/smoke-guides-resize-multipass.txt: 8 GPU frames alternating 256/128 resolution, 1/2/3 passes, changing submission queues and settings; exit 0.
- analysis/smoke-guides-timeout-regression.txt: no-exposure/render-motion path, depth-stencil guide, deliberate timeout, current-input preservation and subsequent recovery; exit 0.

These are standalone backend tests, not a Stellar Blade visual or MFG validation. The native exposure readback rotates over four slots; initial frames use its fallback until those slots contain exposure data. Test image quality and FG separately in the actual game.

## Next game test

Use dxgi.dll. First compare neural on/off with FSR and FG disabled. Then select in-game DLSS and confirm neural passes still complete. Finally enable DLSS FG, close the overlay, return to gameplay, and check OptiScaler.log for `DLSSG options` and `DLSSG replacement evaluate` if the panel still reports OFF.
