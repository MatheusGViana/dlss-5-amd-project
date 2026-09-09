# PCSX2 final-image neural prototype

Restricted to pcsx2-qt.exe and DX12 R8G8B8A8_UNORM output. Enabled with Enable Neural Rendering. It processes the presentation backbuffer before the OptiScaler menu, independently of menu visibility. Game HUD is already part of the input. Dummy zero motion and constant depth 0.5 are generated explicitly, and native history is reset for every frame. This does not provide real geometry/temporal data or demonstrate NVIDIA parity.

One pass, 100% final-image resolution. Lightning Strength is supported; other native SR controls are hidden for this path. Output size/format or queue/device changes require emulator restart in this prototype. BGRA/HDR output is reported unsupported instead of processed incorrectly. Pending work is skipped without reusing old output. Allocator, descriptors and borrowed textures are retained until both native completion and D3D12 fence retirement. No new public package generated.

Tests: dummy-input AMD inference changed synthetic pixels over eight frames with no timeouts; standalone final-image capture/neural/compose processed eight frames with no DirectX12 debug errors. Visual quality, HUD artifacts, menu hidden/visible and PCSX2 integration remain to be tested by running the emulator.
