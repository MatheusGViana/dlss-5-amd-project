# OptiScaler AMD PreSR Multipass v2.7 - test release

Includes the latest Cyberpunk depth compatibility update, frame continuity changes, Experimental effect controls, and additional flicker diagnostics. Intermittent flickering is still under investigation; this release does not claim to eliminate it.

## Install

Close the game, extract the entire package, and run INSTALAR_AMD.bat to open Setup. Select the game executable and proxy DLL name. Setup copies the neural backends, weights, configuration, shaders and dependencies, and backs up existing files. Keep the complete package together to avoid private AMD runtime hash mismatches.

## Changes since v2.6

- Added support for R32G8X24 typeless depth through its depth-plane shader view, used by the tested Cyberpunk path.
- Increased the bounded wait for previously submitted GPU work from 4 to 16 ms. This can reduce skipped neural frames under GPU load but may introduce waiting; it is not a performance improvement guarantee.
- Renamed the optional effect controls to Experimental and Enable effect. It starts disabled and requires neural rendering and AMD pre-SR to remain enabled.
- Log every pending-work and GPU-fence skip, plus temporal-history reset reasons, in amd_presr.log.
- Added amd_bridge.log for bridge state transitions, including warmup and unsupported input states.
- Updated the diagnostic script to include amd_bridge.log.

## Testing intermittent flicker

Compare the same scene with neural rendering enabled and in-game frame generation OFF, then ON. Also compare with Experimental disabled. Report the game, GPU, driver, render resolution/upscaling preset, frame-generation mode, and approximate time of a flicker. Include amd_presr.log, amd_bridge.log, dlssnr_on_amd.log, OptiScaler.log, and OptiScaler.ini when available. Logs are written beside the installed proxy DLL and may grow during long sessions.

## Controls and limitations

Appearance and tonemap retains the existing defaults and reset controls. Enable the appearance filter to use its controls. Tone strength above zero enables its independent tonemap; effect mix zero permits tonemap alone.

Experimental includes quality, denoising, mix, lighting, occlusion, ambient level, thickness, smoothness, fade, camera FOV and depth range. FOV is manual, default 60 degrees; depth range defaults to 1000. Save settings to persist changes. Install the entire rtgi_native folder. Author and license notices remain in Licenses.

The v2.5 timeout changes remain included. Long HIP capture stalls are not resolved. Additional effects consume GPU time and VRAM. Frame-generation compatibility and visual quality require testing per game; this package does not unlock every game's multiframe mode.

The depth compatibility and continuity changes passed targeted synthetic GPU checks. The latest logging changes compiled successfully. These checks do not establish that intermittent in-game flickering is fixed.