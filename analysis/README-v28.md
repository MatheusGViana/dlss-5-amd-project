# OptiScaler AMD PreSR Multipass v2.8 - experimental lighting rewrite

The Experimental effect now uses a newly written local screen-space bounce and occlusion shader, followed by a depth-aware spatial filter and composition. It does not use the former effect's shaders, translated shader functions, blue-noise textures, horizon lookup, radiance-volume pipeline or albedo estimation. Only two compiled shaders are required in experimental_lighting. The existing native DirectX integration is retained.

This is a simpler, visually different effect, not an equivalent reproduction or hardware ray tracing. It can only sample visible screen-space information. There is no temporal accumulation in this effect. Denoiser controls spatial filter radius; smoothness controls depth rejection. Camera FOV and depth range remain manual approximations. Effects may be weak on flat surfaces and cannot illuminate from off-screen objects. In-game quality and GPU cost still need testing.

## Install

Close the game, extract the entire package and run INSTALAR_AMD.bat. Select the game executable and proxy DLL name. Existing files are backed up. Always keep matching AMD backend DLLs and weights together.

Enable DLSS Neural Rendering > Experimental > Enable effect. Neural rendering and AMD pre-SR must be enabled. Experimental starts disabled. Prior rtgi_native files are no longer loaded; this installer does not delete old files. Keep all other required dependencies and license notices.

The separate appearance/tonemap filter and AMD neural backend remain unchanged; this rewrite concerns only the optional Experimental lighting effect.

## Flicker diagnostics

Includes v2.7 continuity changes and diagnostic logs. Intermittent neural flicker remains under investigation. Compare the same scene with frame generation OFF and ON, then Experimental OFF and ON. Include game, GPU, driver, resolution, scaling preset and logs: amd_presr.log, amd_bridge.log, dlssnr_on_amd.log and OptiScaler.log.

## Validation

Synthetic GPU tests cover neutral identity, nonzero effect on curved depth, odd dimensions, finite output, resizing, and typeless depth-plane sampling. No in-game visual or performance claim is made. Long HIP capture stalls remain unresolved.
