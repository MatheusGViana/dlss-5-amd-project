# OptiScaler AMD PreSR Multipass v2.6 - experimental RTGI

RTGI Diffuse is integrated into the AMD pre-SR path without ReShade. Includes depth reduction, reconstructed normals, radiance propagation, tracing, temporal history, denoising, albedo estimation and composition. It runs on rendered SR frames, before upscaling.

Open DLSS Neural Rendering > RTGI Diffuse > Enable RTGI. Neural rendering and AMD pre-SR must remain enabled. RTGI starts disabled. Controls include quality, denoiser, mix, bounce lighting, ambient occlusion, ambient level, thickness, smoothness, fade, camera FOV, depth range and lighting inspection. Save settings to persist [AmdRtgi]. Reset to defaults resets this group.

This first integration is experimental. GPU validation covers synthetic scenes and the AMD bridge; in-game visual quality and frame-generation compatibility still need testing. Camera FOV is manually configurable (default 60 degrees), and depth range defaults to 1000. Geometry normals are reconstructed from depth. Color composition is adapted to pre-exposed linear input. Additional GPU time and VRAM are required; no performance gain is claimed.

Supported depth inputs are R32/R16 float or compatible typeless formats. Typed depth-stencil formats requiring conversion report an RTGI error and preserve neural output. Restart after correcting missing assets or a failed optional effect. Install the entire rtgi_native folder.

The v2.5 timeout change and appearance defaults are retained. The separate long HIP capture stall is not resolved by this release.

Run `INSTALAR_AMD.bat` to open **Setup**. Select the game executable and proxy DLL name. Close the game before installing. Setup installs the OptiScaler proxy, all three matching AMD backend DLLs, weights, configuration and dependencies. Existing files are backed up.

Always distribute/extract the complete package. Copying only OptiScaler.dll over another AMD backend release can cause `Private AMD runtime hash mismatch`. Do not bypass that check: these binaries use a private ABI. Setup now checks the backend files before installation.

## Appearance controls without ReShade

Open **DLSS Neural Rendering â†’ Appearance and tonemap â†’ Enable appearance filter**. Keep AMD neural rendering and pre-SR enabled. Save settings in OptiScaler to persist the controls under `[AmdLook]`.

The appearance filter is off by default and uses one fixed profile. Controls include effect mix, material and shape detail, local lighting, skin detail/softness/mask, specular reduction, highlight roll-off, colour separation, contact shadows, halo/noise protection, and inspection views.

The filter runs once after AMD neural processing, before SR and downstream frame generation. Detail size changes with render resolution. Use **Reset to defaults** to restore all appearance and tonemap values, one neural pass, structure and skin strength 1, and pre-SR enabled. Reset disables the appearance filter. Save settings to retain the reset values after restarting.

## Tonemap

The closed AMD runtime disables its broad lighting/colour channels; its own embedded UI warns that raising its native Tone mostly darkens the image. That native control stays at zero.

The new independent tonemap is functional: set **Tone strength** above zero, then adjust **Exposure (EV)**, **Contrast**, **Saturation** and **Highlight compression**. To use only tonemap, set **Effect mix** to zero. Tone strength zero disables tonemap. Disable the appearance filter to bypass all its processing and extra texture allocation/dispatch on subsequent frames.

## Compatibility and validation

Includes the Stellar Blade physical-adapter detection, FFX scratch-texture barriers, display-resolution motion resampling and float exposure conversion fixes. Stellar Blade's previous build was confirmed working by the user; this release's new visual filter still needs in-game visual evaluation.

The standalone AMD GPU harness tests exposure, resize, multiple neural passes, neutral bypass, timeout recovery and finite output. A numeric tonemap test checks that +1 EV doubles the current input within FP16 tolerance. These are backend tests, not a guarantee of compatibility or image quality in every game. The added spatial filter has a GPU cost when enabled; no game performance claim is made.

DLSS Enabler/frame generation remains separately configured. This package does not claim to unlock every game's multiframe mode. Streamline option and replacement-evaluation diagnostics remain available in OptiScaler.log.
