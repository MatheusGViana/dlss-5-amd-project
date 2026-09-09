# OptiScaler AMD PreSR Multipass v2.17 - neural lighting experiment

Adds Neural lighting (experimental), disabled by default, with strength 0..1 (initial 0.5). This enables the native runtime ToneChannels switch at RVA 0x76e44 and sends the native Tone strength. The controls persist; reset disables the experiment. Existing v2.16 live scale and flicker protection are retained.

Evidence: isolated equal-input GPU tests compared the switch enabled at Tone 0 and 0.5, and disabled at Tone 0.5. Mean RGB: 0.37079, 0.36497 and 0.36063 respectively. Outputs were finite; the enabled switch changes the result. Both nonzero Tone runs slightly darken this synthetic input. This does not establish correct relighting, restored missing kernels, skin isolation or NVIDIA equivalence. No performance-parity claim.

Test first with NR resolution 100%, one pass, structure/character 1, Appearance and Experimental spatial lighting disabled. Compare Neural lighting off/on at 0.25 and 0.5 in the same static scene, then during motion. Check skin, bright signs, shadows, and real rendered FPS. If quality worsens, disable it; no restart is required. Do not compare different game presets or camera exposures as evidence of model parity.

This release uses the existing AMD HIP runtime and weights. It does not install the dxre NVIDIA DLL or RenoDX addon. v2.16 remains the reference release. Full visual evaluation and timing in games are pending.

Installation: extract all files, close game, run INSTALAR_AMD.bat and select executable/proxy. Logs: amd_presr.log, amd_bridge.log, dlssnr_on_amd.log, OptiScaler.log. Other features/defaults remain from v2.16. Existing capture-related crashes and reduced-resolution fidelity limitations are not declared solved.
