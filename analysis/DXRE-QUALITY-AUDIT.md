# DXRE quality / compatibility audit

Inspected dxre/nvngx_dlssnr.dll (165840496 bytes), SHA256 e67dee209320cdafe0e93e45675d7aa34323a53acc57a72b2e40a181581c989a. Version resource reports NVIDIA DLSSNR 310.8.0.0, but Authenticode reports HashMismatch: version strings do not authenticate an unmodified official binary. No execution/AMD compatibility validation performed.

Inspected renodx-dlss.addon64 (2624512 bytes), SHA256 1d855cf226857dce890cffbf7206ba9b6497ce1d471b217c1c8b44b6cd5d27e9. Strings identify RenoDX DLSS controller, ReShade addon lifecycle, direct nvngx runtime attachment and colour/guide/capture transport. This is not evidence of an AMD neural compute implementation. NGX exports on the NVIDIA file do not establish AMD support.

Current AMD runtime's embedded UI explicitly says broad lighting/colour channels are gated off and increasing Tone mostly darkens the frame. Current bridge forces Tone=0 to avoid that known behavior. Thus the separate appearance/tonemap and Experimental effects do not restore missing neural channels. Structural processing alone should not be represented as full model parity.

Current quality reductions: working at a fraction of render size (not display size); reduced-scale contrast confidence attenuates edits to prevent confirmed flicker; pre-SR and post-SR/post-RR use different inputs; character strength is a semantic character channel, not guaranteed isolated skin.

Next engineering targets, in order: establish a controlled reference at equal render/model extent, no optional effects and one pass; inspect native lighting-channel enable flag plus actual HIP pre/post kernels and weights to determine whether channels are merely host-gated or absent/incorrect; only then test channel restoration in an isolated harness. Do not expose a Tone slider merely by setting a flag and call it repaired. Compare full-frame output, highlight stability and GPU duration against v2.16. Reimplement/port missing compute if necessary; replacement of the NVIDIA DLL alone is not a demonstrated solution.

Performance: no evidence for equal NVIDIA fidelity at unchanged cost. Preserve v2.16 as rollback. Upstream hybrid uses NVIDIA-specific compiled kernels in DlssNrNative.cpp; not a drop-in HIP optimization. No production files or installed game were changed during this audit.
