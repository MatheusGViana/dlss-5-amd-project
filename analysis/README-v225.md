# OptiScaler AMD PreSR Multipass v2.25

New Encoding control under DLSS Neural Rendering: Auto (existing), Linear, sRGB and Gamma 2.2. Auto preserves the previous pre-SR path. Linear is an explicit no-transfer override (currently identical to Auto). sRGB and Gamma 2.2 decode source RGB to linear before neural processing, then encode the final output back to the selected transfer. Alpha is preserved. Signed values use a sign-preserving extension. These choices do not implement gamut conversion or HDR tone mapping.

No HDR10/PQ or scRGB-nl compatibility mode is included in this release. Choose Auto for existing working configurations; the selector describes source encoding, not a guaranteed quality preset. Incorrect source selection can change the model response undesirably. Nonlinear modes add two compute passes and two full-input-resolution FP16 textures. Neural history resets when encoding changes; no game restart is required for this setting. Save Settings persists DlssNr/AmdEncoding (0-3). Reset restores Auto.

The failed PCSX2 final-image prototype is disabled in this build. This release does not fix PCSX2 or enable emulator neural support. Retains the v2.24 private compiler correction, Setup.bat and neural disabled by default.

Validation: release build; GPU smoke for sRGB with changing scale/extents/queues and passes, and Gamma 2.2 with a post-submission queue dependency. Visual appearance in individual games remains to be validated.

Transfer reference: https://learn.microsoft.com/en-us/windows/win32/api/dxgicommon/ne-dxgicommon-dxgi_color_space_type
Licenses and notices retained.
GPU transfer round-trip test also passed for alternating sRGB/Gamma 2.2 and resized textures: RGB error below 0.001, alpha unchanged, no debug-layer errors.
