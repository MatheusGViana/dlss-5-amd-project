# Optional NVIDIA DLSS Frame Generation files

This supplies the DLLs for **OptiScaler's own DLSS FG output**, not a new frame-generation
implementation. It does not enable FG, unlock RTX 40 MFG, add native FG support to a game, or
replace the game's existing DLLs. The injected upscaler-to-FG route is experimental.

The pinned source is NVIDIA's [Streamline SDK 2.12.0 release](https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.12.0).
Only its production `bin/x64` files are used: Streamline 2.12.0.0 and `nvngx_dlssg.dll` 310.7.0.0.
This is a reproducible public SDK set, not the newer 2.13/310.8 files used in earlier local tests.
Those earlier tests therefore do **not** validate this particular bundle in a game.

The [manifest](../redist/streamline/manifest.json) pins the official ZIP checksum, each extracted
file's checksum, and the NVIDIA signing certificates. No DLL is patched or downloaded from a mirror.
This change publishes source, a downloader and instructions only: **no NVIDIA FG DLLs or
DLL-containing FG archive are uploaded**. The local bundle from development is not a public release.

## Add the files to an existing OptiScaler install

1. Close the game and its launcher. Back up the OptiScaler installation.
2. Use a package built with `package_release.ps1` that contains `get_streamline.ps1` and
   `redist/streamline/manifest.json`. Older v0.5 and earlier releases do not contain them.
   If using an older release or a nightly without those files, download this repository's
   [source ZIP](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass/archive/refs/heads/main.zip)
   and extract it into a separate working folder. Do not install the source tree over your game.
3. Read the NVIDIA licences below. Open PowerShell in the folder containing `get_streamline.ps1`
   and `redist`, then replace the example path with the real game's executable directory:

   ```powershell
   .\get_streamline.ps1 -Destination 'D:\Path\To\Game\OptiScaler\streamline' -AcceptNvidiaLicenses
   ```

   No administrator rights or antivirus exclusions are needed. The first download is about 232 MB;
   only about 10 MB of runtime DLLs plus licence notices are installed. The SDK remains cached in
   `.dependencies/streamline/2.12.0` beside the script. `-ArchivePath C:\Downloads\streamline-sdk-v2.12.0.zip`
   can use a previously downloaded official ZIP; the same checksum checks still apply.
4. The script refuses a different existing stack or unexpected files in the destination. Do not
   mix DLL versions or move them up beside the game's executable. Keep a known-working stack unless
   you intentionally want to replace it; back up and move that dedicated folder aside first.

Keep this relative layout beside the real game executable:

```text
OptiScaler/
  streamline/
    sl.interposer.dll
    sl.common.dll
    sl.dlss_g.dll
    sl.reflex.dll
    sl.pcl.dll
    nvngx_dlssg.dll
    (four licence/notice files)
```

If you changed `[Libraries] OptiDllPath`, use its `streamline` subfolder instead.
`nvngx_dlssnr.dll`, DLSS Super Resolution and Ray Reconstruction DLLs are not in this component.

## Manual download without the helper

1. Read the [NVIDIA licences](#licences-and-distribution), then download
   [streamline-sdk-v2.12.0.zip directly from NVIDIA](https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.12.0/streamline-sdk-v2.12.0.zip).
   Choose that release asset, **not** GitHub's "Source code (zip)"; the latter does not contain the DLLs.
2. Check the SDK ZIP before extracting it:

   ```powershell
   Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\Downloads\streamline-sdk-v2.12.0.zip'
   ```

   Expected SHA-256: `F5C0A3D870707DDDC3570FB4BCD3655CF48A8A68C3A9D342910CFA21B77DCF48`.
3. Extract the SDK to a separate folder. With the game closed and its setup backed up, copy only
   these six files from the SDK's **`bin/x64`** folder into the game's **`OptiScaler/streamline`**:
   `sl.interposer.dll`, `sl.common.dll`, `sl.dlss_g.dll`, `sl.reflex.dll`, `sl.pcl.dll`, `nvngx_dlssg.dll`.
   Do not use `bin/x64/development`, mix versions, or overwrite the game's native Streamline files.
   Stop if the destination already contains a different set; preserve that working setup first.
4. Keep the following notices beside those DLLs. The helper uses these same destination names:

   | File inside NVIDIA's SDK ZIP | Name in `OptiScaler/streamline` |
   |---|---|
   | `license.txt` | `Streamline-LICENSE.txt` |
   | `3rd-party-licenses.md` | `Streamline-3rd-party-licenses.md` |
   | `bin/x64/nvngx_dlss.license.txt` | `nvngx_dlss.license.txt` |
   | `bin/x64/reflex.license.txt` | `reflex.license.txt` |

5. Check the copied DLL hashes against the [manifest](../redist/streamline/manifest.json).
   `Get-AuthenticodeSignature` must report `Valid` with NVIDIA as the signer for all six.
   Keep antivirus enabled, then follow the FG owner/settings instructions below. The DLLs alone
   do not switch FG on.

## Choose one FG owner

### Game has working native FG / an external RTX 40 MFG unlocker

Keep the game's working Streamline files and use the game's/external mod's controls.
For the external unlocker, follow [RTX40-MFG.md](RTX40-MFG.md) and use `[FrameGen] External=true`.
That option requires a build containing commit `a60f3c93` or later; the v0.5 release does not implement
it. Adding this downloader to an older install does not upgrade its OptiScaler DLL.
That mode deliberately disables OptiScaler's own FG routing, so this optional component is unused.
Do not enable two FG implementations at once.

### Ask OptiScaler to generate frames from the game's upscaler

Start with a supported **Windows DX12** game and an enabled temporal upscaler. NR working by itself
does not prove that FG has the depth, motion vectors, swapchain and pacing it needs. D3D11/Vulkan
bridges and individual games need separate validation. RDR2 DX12 with this bundle is **not tested**.

With the game closed, merge these keys into the existing INI sections (do not replace the whole INI):

```ini
[FrameGen]
External=false
Enabled=true
FGInput=upscaler
FGOutput=dlssg
FGNvngxReplacement=None

[DLSSG]
InterpolationCount=1
ForceDMFG=false
```

Restart. Start with NR off and **2x** FG, then enable one NR pass and test again. `InterpolationCount`
is the number of **generated** frames: 1 means 2x total, 2 means 3x, and 3 means 4x. Try a higher
count only if the GPU/runtime reports support. `OverrideInterpolationCount` targets the game's
native Streamline calls; it is not the setting for OptiScaler's own FG output.

The unmodified NVIDIA runtime supports ordinary DLSS FG on RTX 40/50 and MFG on RTX 50. It does
not itself unlock RTX 40 MFG or provide NVIDIA FG on RTX 20/30. Those are separate compatibility/
replacement paths. A 5080 does not need the RTX 40 unlocker.
Enable Windows Hardware-accelerated GPU scheduling and use a compatible NVIDIA driver.
See NVIDIA's [DLSS FG integration guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.12.0/docs/ProgrammingGuideDLSS_G.md).

If FG fails, keep `OptiScaler.log` and any Streamline log, including the loaded DLL paths and
reported FG support/error. An FPS counter alone is not proof of correct frame generation.
HUD ghosting may require game-specific HUDFix settings; do not enable all experimental options at
once. To revert, set `[FrameGen] Enabled=false`, `FGInput=nofg`, `FGOutput=nofg`, save and restart.
Use single-player games without anti-cheat. Never disable antivirus to make a DLL load.

## Local packaging only

The DLL-containing local build is retained for development. It is **not cleared for redistribution**
by this review. Do not upload it to this repo's releases without resolving the licensing questions below.

In PowerShell 7 on Windows, from this checkout after building OptiScaler:

```powershell
.\package_release.ps1 -Version v0.6.0-fg-preview -SkipBuild -IncludeDlssFrameGeneration -AcceptNvidiaLicenses
```

Omit `-SkipBuild` to build first. `-StreamlineArchive` supplies the official ZIP offline.
The output is `release/OptiScaler-DLSSNR-v0.6.0-fg-preview-with-dlss-fg.zip`. Normal packages omit
the NVIDIA DLLs but include the downloader and manifest. Both variants keep FG and NR off by
default, and include `SHA256SUMS.txt`. Existing version outputs are never overwritten.

## Licences and distribution

The downloader and manifest are project source; NVIDIA's proprietary binaries are **not** added
to Git or relicensed under this repository's GPL. They are fetched directly from NVIDIA. The local
full-package option keeps the original notices beside the DLLs; doing so is not a determination that
the combined package meets either NVIDIA's terms or the repository's GPL.

Read the [Streamline licence](https://github.com/NVIDIA-RTX/Streamline/blob/v2.12.0/license.txt),
[RTX SDK licence](https://github.com/NVIDIA-RTX/Streamline/blob/v2.12.0/external/ngx-sdk/license.txt)
and [Reflex licence](https://github.com/NVIDIA-RTX/Streamline/blob/v2.12.0/external/reflex-sdk-vk/reflex.license.txt).
The exact copies from the pinned ZIP accompany the extracted DLLs, along with third-party notices.
Use of NVIDIA DLSS Frame Generation and NVIDIA Reflex remains subject to NVIDIA's terms.
This project is not endorsed by NVIDIA.

Review outcome (7 September 2026): **use official download links, not a mirrored DLL bundle**.
This is a conservative publishing decision, not legal advice or a claim that all redistribution is
prohibited. The RTX licence grants conditional application redistribution in sections 1(c) and 2,
prohibits a standalone SDK product in 4(b), and restricts subjecting the SDK to open-source terms in
4(e). Its supplement adds notification before commercial release (including a plug-in to a
commercial application) and attribution/marketing requirements. Reflex has separate terms too.

We have not established that this GPL fork's combined DLL package satisfies those requirements.
The Streamline source's permissive licence and valid NVIDIA signatures do not settle that question.
The downloader avoids us redistributing these runtime DLLs; it does not waive NVIDIA's terms for
users or settle every licensing question about an integration. No NVIDIA notification or public
binary upload is performed by these scripts. For redistribution clearance, consult qualified
counsel or the licensing contact listed in NVIDIA's RTX licence.

Checksums and valid signatures establish provenance and detect modification, not that software is
bug-free or guaranteed free of malware. Keep Windows Security enabled and scan downloads normally.
