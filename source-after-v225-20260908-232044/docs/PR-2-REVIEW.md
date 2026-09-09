# Janblade PR #2: window-sized and composition swapchains

Incorporates [janblade's PR #2](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass/pull/2)
with its original commits and attribution. The author reports an in-game NBA 2K26 overlay fix;
that game result has not been independently reproduced here.

Accepted the legacy `CreateSwapChain` 0x0/client-size fix, resolved-size readback and richer overlay
diagnostics. A real window's 0x0 descriptor no longer takes the tiny-helper-swapchain bypass merely
because the requested dimensions are zero. Small/invalid client windows still take the old bypass.

Review adjustments:

- Normalize only the local descriptor before FG/interop creation, so those routes do not receive
  zero dimensions or accidentally resolve against a substituted hidden window. The caller's original
  descriptor remains unchanged. Apply the same sizing helper to the legacy DLSSG factory route.
- Remove the claim that hidden windows necessarily have tiny client areas: visibility and size are
  independent, and a game can legitimately create its swapchain before showing its window.
- Include `CreateSwapChainForComposition`, with all creation paths calling the original trampoline
  exactly once rather than re-entering the detoured virtual method.
- Preserve the composition descriptor and resize flags instead of applying desktop VSync creation/
  resize overrides. Composition requires `FLIP_SEQUENTIAL` and `SCALING_STRETCH`.
- Check creation success before wrapping or updating global state, retain the returned COM/proxy
  chain and reset the new hook pointer if the detour transaction fails.
- Composition does not expose its target HWND at creation. For menu/input initialization, use only
  an eligible current-process foreground window or the sole eligible top-level window in that process.
  If none can be selected, forward Present unchanged and retry later. This is a best-effort association,
  not discovery of the actual DirectComposition/XAML target; multi-window applications need testing.
  This hook does not add a composition-specific FG swapchain replacement.

Reference: [Microsoft's composition swapchain contract](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforcomposition).

`tests/dxgi_window_size_smoke.cpp` checks real hidden client windows, the 100px threshold, tiny/null/
invalid windows, unchanged explicit sizes and preservation of the caller descriptor. It also creates
actual D3D11 WARP swapchains using both raw 0x0 and normalized descriptors and compares `GetDesc()`.
It also checks composition-window eligibility and creates a native composition swapchain, attaches it
to a hidden DirectComposition target, commits and resizes it, verifying dimensions, flags and swap
effect. Both paths passed. This exercises the shared sizing/window helper and native API contracts;
it does not load the modified DLL or prove its detour/wrapper works in a game. In-game injection,
wrapped DirectComposition attachment and FG integration remain unverified.

The x64 Release solution build completed successfully. Existing compiler/linker warnings and legacy
post-build copy messages remain; this is not a claim of a warning-free build or a newly packaged release.

Build/run from a VS x64 Native Tools prompt:

```bat
cl /nologo /std:c++20 /EHsc tests\dxgi_window_size_smoke.cpp /Fe:x64\dxgi_window_size_smoke.exe /Fo:x64\dxgi_window_size_smoke.obj /link d3d11.lib dxgi.lib dcomp.lib user32.lib
x64\dxgi_window_size_smoke.exe
```

The promoted v0.6.1 release retains its original binaries; this later source change requires a new
build. Promotion to release status does not imply additional Dawnwalker or NBA 2K26 game testing.
The subsequent v0.6.2 release supplies that rebuilt package, including both reviewed fixes.
