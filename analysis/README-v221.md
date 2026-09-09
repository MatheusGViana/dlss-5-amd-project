# OptiScaler AMD PreSR Multipass v2.21

Neural rendering is disabled by default in fresh installations. Lightning Strength remains 0.5 when neural rendering is enabled.

HIP worker publication now follows the actual D3D12 ExecuteCommandLists call. Queue binding remains before submission. This avoids launching the capture-wait kernel while its D3D12 capture is still waiting on CPU submission. Multipass ordering, completion fences and timeout protections are retained.

Spider-Man Remastered logs showed roughly 3.4 seconds waiting for capture per frame and an access violation in Spider-Man.exe. This change addresses early worker launch; the crash and in-game recovery are not yet confirmed fixed.

For Spider-Man on AMD, use -forceReflexMarkers in Steam launch options to enable the documented Streamline path while retaining Dxgi=false for ray-tracing compatibility. Select DLSS frame generation in game if exposed, with NvngxFG and the installed Enabler replacement in OptiScaler for MFG. The parameter alone does not establish that MFG is working. Do not enable global DXGI spoofing with ray tracing.
Reference: https://github.com/optiscaler/OptiScaler/wiki/Marvels-Spider%E2%80%90Man-Remastered

Release build and GPU smoke with multiple passes, queue changes and scale/extent changes passed. Full game testing remains necessary. Extract all files and run Setup.bat with the game closed. License notices retained.
