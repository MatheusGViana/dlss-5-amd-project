# OptiScaler AMD PreSR Multipass v2.9 - experimental lighting rewrite

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

## v2.9 defaults and controls

Defaults match the requested preset: Quality Medium (2), Denoiser Medium (1), mix 1, bounce lighting 5, ambient occlusion 1, ambient level 1, thickness 0.1, smoothness 0.5, fade 0.3, FOV 60, depth range 600, final image inspection. The effect starts disabled. Saved settings remain respected; Reset to defaults restores this preset and disables the effect, which can then be enabled again.

New original controls: Contact shading (default 0) adds short-range nondirectional cavity shading; it is not a shadow cast by a known game light. Bounce saturation (default 1) adjusts indirect light colour. Sample radius (default 1) controls gather reach. New controls are neutral by default to preserve the requested preset, and are saved in the configuration.

Twelve synthetic GPU cases passed including neutral output, contact-only shading, saturation/radius variations and odd dimensions. In-game evaluation is still required.