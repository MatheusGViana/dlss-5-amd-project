# v2.15 investigation and containment

Native decompilation (analysis/inference-decompile.txt around 731-741) sets global structure separately from the semantic character channel. SkinStructure=0 does not zero the global structure channel. UseAutoMask at RVA 0x76e40 gates the separate character channel. The bridge now explicitly enables it and the UI calls the control AMD character structure. This is not pixel-perfect skin isolation. Achieving that requires an accessible validated mask and separate compositing, which are not implemented.

NR scale is latched on the first game bridge evaluation and cannot rebuild the model via slider edits during that session. Save settings and restart to apply. This contains the reported live-scale trigger; game resolution changes and the underlying native capture stall remain possible. The log associates a ~2 second capture wait and watchdog event with a subsequent 0x80000003 exception in Cyberpunk's executable, not a diagnosed pointer fault in the slider. No root-cause crash fix is claimed.

The v2.13 strength loss is caused by contrast-dependent confidence attenuation and residual clamping. Retained unchanged: simply lifting these limits risks the confirmed prior light flicker. 100% avoids this reduced-scale attenuation. Restoring equal strength and temporal stability at 25% remains unimplemented, not silently declared fixed.

Validation: native backend smoke test at 50% and forced timeout passed. No semantic skin reference images or in-game crash reproduction were available for validation. This release is a containment test, not completion of all three issues.

# OptiScaler AMD PreSR Multipass v2.15 - NR scaling and upstream compatibility

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

## v2.15 reduced-scale correction

Replaces single-sample bilinear reduction with source pixel-footprint area averaging to reduce aliasing of fine emissive details. At 100%, uses an exact texel load. Fixes per-frame input logging while model resolution differs from input resolution.

Severe reductions still discard model input detail: 27% retains about 7.3% of the input pixel count. This is not equivalent quality to a full-resolution neural pass. Begin visual comparisons at 100%, structure/skin 1, optional effects off, then compare 75% with all other settings unchanged. Existing settings are not forcibly reset. Flicker may have additional causes and is not declared fixed.

GPU smoke checks passed at 27% with forced timeout preservation, at 50% with multipass/resize/queue changes and the optional effect, and at 100%. These validate execution and do not replace in-game motion and quality evaluation.
## v2.15 resolution transition handling

The NR slider commits after release/text-edit completion, avoiding repeated HIP model rebuilds during a drag. After input extent or NR scale changes, the game bridge requires 300 ms of stable settings before recording neural processing. During this interval the current game image continues without the neural effect. Continuous dynamic resolution changes can prolong this interval; fixed resolution/presets are preferable for this test release.

Input-size changes now reset guide history even when the resulting neural dimensions coincide. The game's SR preset and NR percentage remain separate: model size is the current input size multiplied by NR percentage, not the display size. No GPU watchdog was disabled or extended.

Combined synthetic test alternates game input extent and NR scale (100/50/25/75%) alongside multipass, queue changes and typeless depth. This does not reproduce the game's real capture scheduling; 500 ms capture spikes and all in-game timeouts are not claimed fixed.
## v2.15 conservative reduced-scale composition

Reduced-scale residual edits are attenuated where the current full-resolution pixel disagrees strongly with the reconstructed low-resolution input. Residual magnitude is bounded relative to the local input. This protects small bright features and limits spill across mixed pixels, at the cost of a weaker neural effect on high-contrast areas. No previous-frame image is reused. The 100% path is unchanged. This is a spatial mitigation, not a validated temporal flicker fix.

The reported crash is NOT resolved: the available native log records a roughly 2-second capture wait and watchdog event before exception 0x80000003 in Cyberpunk2077.exe. It occurs at native job 116 after a staging resize, so the log does not establish an immediate slider-handler failure. The slider still commits only on release. Restart-based testing at one fixed scale is appropriate until live transitions are verified.

Synthetic 25% forced-timeout test passed with exact current-input preservation. In-game validation is required before treating this as a stable release.
# Tester log review / v2.15

- nb2k26: actual executable folder NBA 2K27; OptiScaler build 20260907_184739, Radeon RX 9060 XT. Not identified as v2.13. Native log has standalone hook startup followed by another load; cannot conclude duplicate active processing without module inventory. Exposure and motion vary substantially, but this alone does not prove a bug. No validated flicker fix for this game.
- cyber2: actual Spider-Man 2. Encoded mean repeatedly zero, inferred exposure ~10000; investigate colour capture/exposure with matching OptiScaler and bridge logs. Do not assume Cyberpunk or invent missing 4x4 motion vectors.
- slide neuro: full-size motion format 10 (RGBA16F) rejected by the resampler whitelist. Added RGBA16F/RGBA32F support; XY is read by the existing shader. Synthetic GPU test covers RGBA16F with nonzero XY, scale/extent changes, multiple passes and optional effect.
- transient small guides: preflight rejection before GPU recording now returns without permanent backend failure; next valid input may recover. No fabricated vectors.
- skin: bridge writes separate structure and skin native fields, but the runtime's global structure behavior is not a skin exclusion contract. Independent skin suppression is unresolved; no skin-segmentation claim.
- intensity: v2.13 protection deliberately suppresses reduced-resolution edits at high contrast. Retained because user reports flicker stopped. Recovering strength without flicker remains unresolved.

The crash reported during slider interaction is not declared fixed. Need matching runtime/module version and crash stack for further attribution.
