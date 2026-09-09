# OptiScaler AMD PreSR Multipass v2.19 - shared frame stabilization

Fixes inconsistent stabilization across worker threads: the single AMD backend now uses shared frame extent/stability state, serialized with a mutex. Thread-local replacement parameter ownership is retained. Resolution/scale settling remains 300 ms; no watchdog or retry protection was removed.

GTA tester logs identify RX 7900 XT, 2294x960/2133x893 neural extents and up to two passes. They contain 10 one-second timeout recoveries, 20 GPU fence skips and bridge messages from 8 threads. These logs establish timeout-driven effect interruptions, but lack native worker timing and matching full configuration/build identity. The shared-state fix can prevent unnecessary per-thread warmup/history resets; it does not establish that compute/capture timeouts are solved.

For GTA evaluation, use one pass, NR 75% initially, fixed game render resolution, then compare FG off/on. Include the consolidated diagnostic report, especially dlssnr_on_amd.log, to distinguish capture wait from inference cost. Avoid claiming a universal five-second cycle from the current logs.

Upstream v0.6.2 swapchain corrections are already present: DxgiFactory_Hooks.cpp matches the supplied Update source byte-for-byte. These are window-sized and DirectComposition compatibility fixes, not a new neural resolution/performance mode. The user clarified that Update is the intended project, not a separate Autopilot folder.

Release build verified; in-game GTA behavior and threaded engine integration need testing. Retains v2.18 controls, real-time release-to-apply scaling, lighting and reduced-scale highlight protection. No new quality/performance equivalence claim.

Extract complete package and run INSTALAR_AMD.bat with game closed. COLETAR_LOGS.bat creates OptiScaler-Diagnostics.txt for support. Attribution and license notices retained.
