# OptiScaler AMD PreSR Multipass v2.12 - NR scaling and upstream compatibility

## Independent neural resolution

NR resolution (%) now controls the AMD model input between 25% and 100% per dimension, independently of the game's rendering/upscaling preset. 50% runs the model at one quarter of the input pixel count, subject to a 32-pixel minimum per dimension. This is not a claim of proportional FPS gain. 100% is the default.

At reduced scale, colour is resampled, depth and motion guides are remapped, and motion pixel scales are adjusted. The reduced input is retained; the processed result minus that input is resized and added to the current original-resolution image. The game's output size is preserved. Optional appearance and Experimental effects operate at the neural working size on this path. Smaller working sizes may reduce detail or introduce edge artifacts. Scale changes reset neural history. Depth formats that cannot be sampled are rejected; restore 100% and restart after an error.

AMD always processes before Super Resolution. The old checkbox was removed because disabling it stopped neural rendering without switching to an implemented post-SR path. Legacy RunBeforeSR=false no longer silently disables this AMD path. Native Ray Reconstruction/post-RR remains unsupported by this backend; this release does not implement the NVIDIA after-RR slider.

## Selected upstream changes

Compared against the source supplied in Update, including v0.7.1 changes:
- Resolve Streamline functions from active initialized plugins, instead of assuming the bundled plugin remains active after overrides.
- Respect older DLSSG state structure sizes, stop on failed state reads, and keep capability clamping temporary rather than overwriting saved interpolation preferences.
- Retain AMD-specific FG diagnostics and guide/queue fixes.
- The window-sized swapchain and padded active-input fixes were already in our base.

Not imported into AMD: Blackwell FP8/NVFP4 hybrid kernels, NVIDIA residual-frame-generation experiments, Ada MFG patching, or a new Vulkan HIP backend. Those are not compatible drop-in optimizations for this private AMD runtime. This is a selected adaptation, not a complete replacement of the fork by upstream.

Upstream attribution: https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass/releases

## Install and test

Close the game, extract all files, run INSTALAR_AMD.bat and choose the executable/proxy. Includes all v2.9 effects and defaults. Compare NR resolution 100%, 75% and 50% at an unchanged game resolution/preset. Record visual quality, real rendered FPS, and behavior with FG off/on. Synthetic GPU tests passed for 100%, 50%, 25%, resize, multipass, queue changes, exposure/motion conversion, typeless depth and forced timeout input preservation. No in-game FPS or flicker fix is claimed. Existing neural flicker diagnostics remain enabled.

## v2.12 reduced-scale correction

Replaces single-sample bilinear reduction with source pixel-footprint area averaging to reduce aliasing of fine emissive details. At 100%, uses an exact texel load. Fixes per-frame input logging while model resolution differs from input resolution.

Severe reductions still discard model input detail: 27% retains about 7.3% of the input pixel count. This is not equivalent quality to a full-resolution neural pass. Begin visual comparisons at 100%, structure/skin 1, optional effects off, then compare 75% with all other settings unchanged. Existing settings are not forcibly reset. Flicker may have additional causes and is not declared fixed.

GPU smoke checks passed at 27% with forced timeout preservation, at 50% with multipass/resize/queue changes and the optional effect, and at 100%. These validate execution and do not replace in-game motion and quality evaluation.
## v2.12 resolution transition handling

The NR slider commits after release/text-edit completion, avoiding repeated HIP model rebuilds during a drag. After input extent or NR scale changes, the game bridge requires 300 ms of stable settings before recording neural processing. During this interval the current game image continues without the neural effect. Continuous dynamic resolution changes can prolong this interval; fixed resolution/presets are preferable for this test release.

Input-size changes now reset guide history even when the resulting neural dimensions coincide. The game's SR preset and NR percentage remain separate: model size is the current input size multiplied by NR percentage, not the display size. No GPU watchdog was disabled or extended.

Combined synthetic test alternates game input extent and NR scale (100/50/25/75%) alongside multipass, queue changes and typeless depth. This does not reproduce the game's real capture scheduling; 500 ms capture spikes and all in-game timeouts are not claimed fixed.