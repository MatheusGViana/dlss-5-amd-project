# Padded input fix for pre-SR Neural Rendering

Previously, pre-SR required the reported active colour size to exactly equal the resource allocation.
A game rendering 2558x1439 into a 2560x1440 texture would therefore fall back after SR. At 4K output
and `WorkingScale=1`, NR then processed 4K, even though DLSS itself could still be upscaling correctly.
This is a plausible explanation for the Dawnwalker report, not a reproduced game diagnosis.

The DX12 path now accepts valid origin-zero active rectangles inside larger single-sample 2D colour
textures. It copies the live image to a compact UAV texture, runs the existing NR pipeline there,
then copies only that rectangle back. No resampling is added by these copies. Right/bottom padding
is left untouched, including when the game colour texture has no UAV support. Model resolution is
the active size times `WorkingScale`, not the allocation or DLSS preset name.

The compact texture is reused and retired with NR's other work textures on size/format/placement
changes. Model history and each multipass feature are rebuilt when the active size changes. Existing
hold, comparison, reduced-resolution and capture paths see the compact image rather than stale
padding. The staging copies are included in the displayed NR GPU cost. Frequent dynamic-resolution
changes can still cause rebuilding overhead; this does not promise stutter-free DRS.

Non-zero colour origins, incomplete/out-of-bounds active sizes, arrays and multisampled inputs still
take the guarded post-SR path. Missing active dimensions (both zero) keep the previous allocation-size
interpretation. Native RR still uses its separate post-RR controls; native Vulkan is unchanged.

## Validation

`tests/nr_active_color_smoke.cpp` uses D3D12 WARP and the same extent/copy helpers as production. It
checks every active pixel after cropping and every original allocation pixel after copy-back:

- 2558x1439 inside 2560x1440, with and without UAV support on the game texture.
- 2227x1253 inside a 3840x2160 maximum-size allocation.
- Exact 1920x1080 input, plus a simulated failed evaluation that must not copy an edit back.
- Rejection of partial, out-of-bounds, offset, multisampled and array inputs.

From a VS x64 Native Tools prompt at the repository root:

```bat
cl /nologo /std:c++20 /EHsc tests\nr_active_color_smoke.cpp /Fe:x64\nr_active_color_smoke.exe /Fo:x64\nr_active_color_smoke.obj /link d3d12.lib dxgi.lib
x64\nr_active_color_smoke.exe
```

These tests passed locally. The system D3D12 debug layer was unavailable; the test reports this and
still verifies GPU readback values. This test does not load the NVIDIA NR runtime or a game.
Dawnwalker and NVIDIA's acceptance of its exact odd-sized model inputs still need in-game validation.

## Confirm the game is using it

Use a new build of the actual loaded OptiScaler proxy (for example `dxgi.dll`), not just a replaced
unused `OptiScaler.dll`. Use the v0.6.1 padded-input preview or a newer build; v0.5 and earlier
do not acquire the fix from new README/INI files.
Start with one NR pass, **Apply before Super Resolution** on, model resolution at 100%, and RR/FG off.
Switch Performance to Balanced and check `OptiScaler.log` for:

- `DLSS-NR before SR: staging active ... from padded Color allocation ...`
- `DLSS-NR running before SR: target 2558x1439, model 2558x1439, ...`

The staging message is reported on the first padded input; the running/model dimensions are logged
when the NR feature is rebuilt. The older exact-allocation-size fallback should no longer occur.
If `Falling back after SR` still appears, its dimensions/origin identify the remaining unsupported
layout. If model creation/evaluation fails at the odd size, that is a separate runtime issue: send
the full log, GPU/driver, runtime hash, OptiScaler build and INI. This fix does not force a false
"standard" input resolution or silently stretch the game's image.
