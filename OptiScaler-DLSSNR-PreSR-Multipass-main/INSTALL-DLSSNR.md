# Install DLSS Neural Rendering

This fork is experimental. Do not use injection mods in anti-cheat-protected multiplayer games.

## Requirements

- A 64-bit game whose temporal upscaler reaches an OptiScaler D3D12 path. Native D3D12 is preferred;
  supported D3D11 and Vulkan games can use OptiScaler's D3D12 bridges.
- NVIDIA driver 616.56 or newer.
- The complete release archive from this repository. It includes `setup_windows.bat`, the
  `OptiScaler` backend folder, `OptiScaler.dll`, `OptiScaler.ini`, and `nvngx.dll_dlssnr.dll`.
- A separately obtained `nvngx_dlssnr.dll` 310.8 runtime appropriate for the GPU.

The two similarly named files are different and both are required:

| File | Purpose |
|---|---|
| `nvngx.dll_dlssnr.dll` | Open-source forwarder supplied by this project |
| `nvngx_dlssnr.dll` | NVIDIA-derived Neural Rendering runtime supplied separately by the user |

## Choose the correct runtime

| GPU | Runtime | SHA-256 |
|---|---|---|
| RTX 50 | Original NVIDIA-signed 310.8 | `E16BCF15E16E13F527491CDF7845B2FE6521A738D8F7C9C721866A8496E1FC8E` |
| RTX 20 / 30 / 40 | ShortFuse cross-generation 310.8 | `E67DEE209320CDAFE0E93E45675D7AA34323A53ACC57A72B2E40A181581C989A` |

For RTX 20/30/40, obtain the compatibility runtime from
[ShortFuse's pinned RenoDX thread](https://discord.com/channels/1408098019194310818/1543976771920330884).
The compatibility DLL automatically selects an FP16-oriented path on RTX 20/30, an Ada-compatible
path on RTX 40, and leaves the RTX 50 path unchanged.

The compatibility runtime is modified, so Windows reports the original NVIDIA signature as invalid.
That is expected for this exact hash, but it removes the assurance provided by Authenticode. Keep
security protection enabled, use only the pinned developer attachment, and verify the SHA-256 value:

```powershell
Get-FileHash .\nvngx_dlssnr.dll -Algorithm SHA256
```

## Install

1. Close the game and its launcher.
2. Find the directory containing the real game executable, which is often below the game's root.
3. Back up any existing proxy DLL, `OptiScaler.ini`, and OptiScaler installation.
4. Extract the entire release archive into that executable directory. Do not copy only the two DLLs.
5. Put the correct `nvngx_dlssnr.dll` from the table above in the same directory.
6. Run `setup_windows.bat`. It renames `OptiScaler.dll` to a proxy filename the game will load and
   creates an uninstaller. `dxgi.dll` is the usual first choice. The validated Cyberpunk 2077 setup
   used `dbghelp.dll` to coexist with its existing loaders.
7. Enable Neural Rendering in the `Insert` overlay, or edit `OptiScaler.ini`:

```ini
[DlssNr]
Enabled=true
RunBeforeSR=true
Passes=1
WorkingScale=1.0
```

Begin with one pass. RTX 20/30 use a much heavier FP16 path, so reduced model resolution may be
necessary. With `RunBeforeSR=true`, DLSS Performance at 3840x2160 gives the model a 1920x1080 input
before Super Resolution. `WorkingScale=0.5` lowers only the model's work resolution further.

For a portable setup, leave the process filter disabled:

```ini
[ProcessFilter]
TargetProcessName=auto
```

Do not copy an INI containing another game's executable name. A mismatch intentionally puts
OptiScaler into pass-through mode, which means no menu and no Neural Rendering.

## Game notes

- **Baldur's Gate 3:** install beside `bg3.exe` / `bg3_dx11.exe` in `Baldurs Gate 3\bin`.
  Use `Dx12Upscaler=dlss` for `bg3.exe`, or `Dx11Upscaler=dlss_12` for `bg3_dx11.exe`.
- **Hogwarts Legacy:** install in `Phoenix\Binaries\Win64`; `dxgi.dll` was validated.
- **Cyberpunk 2077:** install in `bin\x64`; `dbghelp.dll` was validated on the development machine.
  Existing CET/RED4ext/ReShade loaders can require a different proxy or correct chaining.

Do not install the RenoDX DLSS add-on merely to obtain its compatibility runtime. This OptiScaler
fork drives `nvngx_dlssnr.dll` itself, and two Neural Rendering injectors can conflict.

## Individual pass controls

Under **DLSS Neural Rendering → Model passes**, expand Pass 1, Pass 2, or Pass 3. Each contains
Style, Intensity, Local structure, Local tone, Skin structure, and Auto skin mask. Sliders commit
when released to avoid rebuilding the model on every movement. Set the model pass count to 2 or 3
to activate later passes; editing inactive passes prepares their settings without running them.

Later passes inherit pass 1 unless overridden, except Local tone, which defaults to 0 to preserve
the earlier build's appearance. Reset on a later-pass slider clears its override. Intensity,
local structure, and local tone range from 0 to 2; skin structure ranges from -1 to 2, with -1
following local structure. The corresponding INI keys are `Pass2Intensity`, `Pass2LocalStructure`,
`Pass2LocalTone`, `Pass2SkinStructure`, and `Pass2AutoMask`, with matching `Pass3...` keys.
Use `auto` for the default behavior. Styles retain `Pass2Style` / `Pass3Style`.

These controls apply to D3D12 multipass and its bridges, both before/after SR and after native RR.
Native Vulkan and the driver-proxy backend remain single-pass. Preset hints are still transmitted
at model creation, but a changed hint is not proof of a changed model. They are preserved under
**Advanced preset hints (effect unverified)** and in the INI for compatibility.

## Padded DLSS input sizes

With a build containing the padded pre-SR fix and `RunBeforeSR=true`, an origin-zero 2558x1439 image
inside a 2560x1440 colour texture runs NR at 2558x1439 when `WorkingScale=1`. The padding is not
processed or overwritten. This does not change the game's DLSS preset ratios. Non-zero colour
offsets and invalid rectangles still fall back after SR. The older release downloads do not gain
this fix through an INI change; the loaded OptiScaler proxy DLL must be updated.
See [validation and reporting instructions](docs/PADDED-PRESR.md).

## Neural Rendering with native Ray Reconstruction

In a game that already supports RR, enable RR in the game's settings and enable
**Apply after Ray Reconstruction (DX12)** in OptiScaler's Neural Rendering menu. The master
**Enable Neural Rendering** switch must also be on. Equivalent INI settings:

```ini
[DlssNr]
Enabled=true
ApplyAfterRR=true
RRPasses=1
RRWorkingScale=0.5
```

RR reconstructs and upscales first. NR then processes that output before frame generation.
`RunBeforeSR` does not override this order. At 4K output, `RRWorkingScale=0.5` runs NR at
1920x1080 and resizes its edit for composition; it does not reduce RR's own resolution.
`RRPasses=1..3` is independent of ordinary `Passes`, while per-pass model profiles are shared.
Switching between SR and RR rebuilds NR history even when their dimensions match.
These controls apply to D3D12 and its bridges, not the upstream native Vulkan NR path.

If Cyberpunk's RR option is greyed out with this fork, avoid the `d3d12.dll` proxy: an
[upstream report](https://github.com/Dagherbou/OptiScaler_DLSSNR/issues/8) confirmed that using
`dxgi.dll` resolved a Streamline conflict. Back up existing loaders before changing the proxy.
Keep the game's genuine `nvngx_dlssd.dll` (RR) separate from `nvngx_dlssnr.dll` (NR).
For an RR-only comparison, disable the master NR switch; “Apply the model” merely hides the edit
and still incurs NR's GPU cost. Successful RR initialization alone does not prove image quality.

## Optional DLSS Frame Generation

For the six NVIDIA Streamline/FG dependencies, the pinned download command, and separate instructions
for native/external FG versus OptiScaler's own FG, see [DLSS-FRAME-GENERATION.md](docs/DLSS-FRAME-GENERATION.md).
Do not copy another game's Streamline folder or assume NR working proves FG compatibility.
The optional component does not include the NR model or enable FG automatically.

## Diagnose a missing menu

Set:

```ini
[Log]
LogToFile=true
LogLevel=2
```

Then launch into a rendered scene and press `Insert` (`Alt+Insert` can help on some keyboard layouts).

- No `OptiScaler.log` beside the executable: the proxy was not loaded. Check the directory, proxy
  filename, antivirus quarantine, and conflicts with another DLL using the same proxy name.
- The log says `OptiScaler ... loaded` and `working as ...`: injection succeeded. A remaining problem
  belongs to the overlay input or Neural Rendering initialization, not the loader.
- `the model would not initialise`: on RTX 20/30/40, first check that the runtime hash is the
  compatibility `E67DEE...` build rather than the original `E16BC...` build.
- The menu toggles but does not accept input: try `[Hotfix] ManualInputPolling=true` and test without
  conflicting overlays.
