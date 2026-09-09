# OptiScaler DLSS-NR pre-SR multipass fork

> [!IMPORTANT]
> This is an experimental fork of [Dagherbou/OptiScaler_DLSSNR](https://github.com/Dagherbou/OptiScaler_DLSSNR), based on commit [`97376162`](https://github.com/Dagherbou/OptiScaler_DLSSNR/commit/973761621353b99bee3dc7d4bb27b117fef2644f) (`v0.2.0-dlssnr` / `v0.2.0-patch1`). It is not the main OptiScaler project and is not supported by NVIDIA or game developers.

The upstream fork already provided experimental direct access to NVIDIA DLSS Neural Rendering. This fork adds:

- **Optional Neural Rendering before DLSS Super Resolution.** The model can process the DLSS input image—such as 1920x1080 in 4K Performance mode—before DLSS upscales it to the display resolution.
- **Configurable multipass processing.** `[DlssNr] Passes=1..3` runs one, two, or three sequential neural evaluations. Each pass has independent persistent history; the final result is composed once against the original base image.
- **Per-pass model profiles.** Passes 2 and 3 can inherit pass 1 or select their own built-in preset and style (`standard`, `natural`, or `cinematic`) without loading competing model DLLs.
- **Independent model strengths per pass.** Each pass has intensity, local structure, local tone,
  skin structure, and auto skin mask controls. Preset hints are grouped under a collapsed advanced
  section because their visual effect is unverified; style is the primary profile selector.
- **Padded pre-SR inputs.** Origin-zero active images inside larger colour textures run NR at the
  active resolution. Ray Reconstruction, non-zero colour offsets and invalid rectangles retain the
  post-SR fallback. This is not restricted to standard 1080p/1440p/4K sizes.
- **Matching overlay and INI controls.** `RunBeforeSR` and `Passes` are exposed in both configuration and the OptiScaler overlay.
- **Optional NR after native Ray Reconstruction (DX12).** Enable `ApplyAfterRR` separately;
  `RRPasses` defaults to 1 and `RRWorkingScale` to 0.5 of RR's output dimensions. RR keeps its
  original noisy inputs, and NR history is rebuilt when switching between SR and RR.
- **Verified BG3 path.** Baldur's Gate 3 was tested through the `bg3_dx11.exe` D3D11-to-D3D12 bridge with two neural passes at 1920x1080 followed by DLSS Super Resolution to 3840x2160.

Downloads:

- [Latest release — v0.6.2](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass/releases/tag/v0.6.2-swapchain-fixes) — rebuilt complete package including janblade's window-sized swapchain fix and the reviewed DirectComposition hook, plus the previous padded pre-SR and skin/MFG changes. Build and native API smoke-tested; in-game validation of the new hooks is pending. NVIDIA NR/FG runtimes are not bundled.
- [Per-pass controls preview](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass/releases/tag/v0.5.0-pass-controls-preview) — reorganized pass sections and independent model strengths, including the RR controls. Runtime validation is pending.
- [Native RR controls preview](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass/releases/tag/v0.4.0-rr-preview) — compiled experimental build with independent NR-after-RR controls. In-game RR/NR validation is pending.
- [Portable cross-generation package](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass/releases/tag/v0.3.0-crossgen-portable) — the complete installer and backend layout, with game-neutral defaults and RTX 20/30/40/50 runtime guidance.
- The earlier `general-per-pass-profiles-facc24f6` and `bg3-presr-multipass-e16d5866` packages are retained only as historical validation artifacts. They are incomplete for a clean installation and should not be redistributed.

NVIDIA's proprietary `nvngx_dlssnr.dll` is required but is **not redistributed** here.

### Optional DLSS Frame Generation dependencies

**The NVIDIA Streamline/FG DLLs are not hosted in this repo or uploaded with this change.**
The DLSS runtime has separate redistribution conditions; the Streamline source licence does not
cover the whole DLL set. See the [licence review and official sources](docs/DLSS-FRAME-GENERATION.md#licences-and-distribution).

Get the files from NVIDIA's [Streamline 2.12.0 release](https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.12.0)
([official SDK ZIP](https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.12.0/streamline-sdk-v2.12.0.zip)).
Our helper downloads that exact ZIP and verifies its checksum, all six DLL hashes and NVIDIA signatures.

1. Install a complete OptiScaler release first. The v0.6.1 release includes this helper; v0.5 and earlier do not.
2. If your release lacks the helper, download this repo's [source ZIP](https://github.com/wilsjo2/OptiScaler-DLSSNR-PreSR-Multipass/archive/refs/heads/main.zip)
   and extract it into a **separate working folder**, not over the game. Open PowerShell in the
   folder containing `get_streamline.ps1` and `redist` (the installed release folder if already present).
3. Close the game, back up its OptiScaler setup, read the linked NVIDIA licences, then run the
   following command with your game's real executable directory in place of the example:

   ```powershell
   .\get_streamline.ps1 -Destination 'D:\Path\To\Game\OptiScaler\streamline' -AcceptNvidiaLicenses
   ```

4. Follow the [FG setup instructions](docs/DLSS-FRAME-GENERATION.md#choose-one-fg-owner).
   Downloading the DLLs does **not** enable FG or unlock RTX 40 MFG. Existing different DLLs are
   never overwritten. Do not replace a game's working native Streamline stack.

Prefer manual installation? The [manual download instructions](docs/DLSS-FRAME-GENERATION.md#manual-download-without-the-helper)
list the exact production files and their destination. The source ZIP is not a compiled OptiScaler
release. This supplies dependencies, not a guarantee of injected FG compatibility in every game.

### New compatibility work (not yet game-validated)

- v0.6.2 incorporates janblade's NBA 2K26 window-sized swapchain fix, diagnostic
  logging and a reviewed DirectComposition hook. [Review and validation](docs/PR-2-REVIEW.md).
  These changes are included in the v0.6.2 ZIP; the historical v0.6.1 ZIP is unchanged.
- Padded pre-SR colour inputs, including the reported 2558x1439-in-2560x1440 case, no longer fall
  back to 4K NR merely because the allocation is larger. [Details and testing](docs/PADDED-PRESR.md).
- Optional skin-colour protection with separate skin/environment lighting and colour controls.
  This is a colour-based filter, not a face detector. The model's AutoMask remains a separate control.
- An **External frame generation / MFG unlocker** mode that leaves Streamline and Reflex to the game
  or external mod. [RTX 40 unlocker instructions and optional pinned source build](docs/RTX40-MFG.md).
- The retail Onimusha executable gets the same engine-state workaround as the demo. NR model creation
  is now inside the graphics-state restore envelope as well as evaluation.

See [what changed, limitations and tests](docs/NR-COMPATIBILITY.md). These changes are not in the older
v0.5 download listed above. The RTX 40 unlocker is a separate optional component, not enabled by default.

### GPU and runtime compatibility

The Neural Rendering network can run on RTX 20, 30, 40, and 50-series GPUs, but not with the same
runtime binary on every architecture:

| GPU | Required `nvngx_dlssnr.dll` 310.8 runtime | SHA-256 |
|---|---|---|
| RTX 50 | Original NVIDIA-signed runtime | `E16BCF15E16E13F527491CDF7845B2FE6521A738D8F7C9C721866A8496E1FC8E` |
| RTX 20 / 30 / 40 | ShortFuse cross-generation compatibility runtime from the pinned RenoDX thread | `E67DEE209320CDAFE0E93E45675D7AA34323A53ACC57A72B2E40A181581C989A` |

The compatibility runtime preserves the model but replaces or backports GPU programs that the older
architectures cannot execute: the RTX 20/30 path is predominantly FP16, while the RTX 40 path
backports Blackwell-only operations. It is a modified NVIDIA-derived binary, so its original NVIDIA
Authenticode signature no longer validates. Obtain it only from
[ShortFuse's pinned RenoDX thread](https://discord.com/channels/1408098019194310818/1543976771920330884)
and verify the hash above. Do not use random DLL mirrors. Driver 616.56 or newer is required.

### Quick install

1. Close the game, then back up any existing OptiScaler/ReShade proxy DLL and INI files.
2. Extract the **entire** release archive beside the game's real 64-bit executable. Keep the
   `OptiScaler` and `Licenses` directories with the DLLs; copying `OptiScaler.dll` alone will not work.
3. Add the GPU-appropriate `nvngx_dlssnr.dll` from the table above to that same directory and verify
   its SHA-256. Both it and this package's differently named `nvngx.dll_dlssnr.dll` must be present.
4. Run `setup_windows.bat` from that directory. Choose `dxgi.dll` first unless the game or another
   loader already uses that name, and answer **NVIDIA** when prompted. The script renames
   `OptiScaler.dll` to the selected loadable proxy and confirms which Neural Rendering runtime it found.
5. Leave `[ProcessFilter] TargetProcessName=auto` for a portable installation. Start the game, enter a
   rendered scene, press `Insert`, and enable **DLSS Neural Rendering**. Start with one pass.
6. For pre-upscale operation, set `RunBeforeSR=true` and select DLSS in the game. At 3840x2160 output,
   DLSS Performance supplies a 1920x1080 input to the model before Super Resolution.

If no menu or `OptiScaler.log` appears, the proxy did not load: re-check the executable directory,
proxy filename, antivirus quarantine, and conflicts with an existing loader. Do not copy an INI whose
`TargetProcessName` names a different game; that deliberately activates pass-through mode.

Read [INSTALL-DLSSNR.md](INSTALL-DLSSNR.md) for the full instructions, per-game paths, loader notes,
configuration example, and diagnostics.

Implementation details and safety invariants are documented in [the pre-SR multipass design note](OptiScaler/dlssnr/design/pre-sr-multipass.md). The remainder of this README is the upstream OptiScaler documentation.

### Compatibility scope

The implementation contains no BG3-specific executable names, offsets, or shaders. It is designed for 64-bit games whose DLSS Super Resolution call reaches OptiScaler's Direct3D 12 path, including its Direct3D 11/Vulkan-to-DX12 bridges. It has also run in Hogwarts Legacy and Cyberpunk 2077. Compatibility still depends on the game exposing valid colour, depth, motion-vector, resolution, and command-submission data through its upscaler integration.

Ray Reconstruction deliberately uses the post-SR path. Native Vulkan currently retains the upstream post-SR implementation. Games with unusual loaders, multiple swapchains, non-zero colour offsets, invalid render rectangles, anti-cheat, or another `dxgi.dll` mod may need a different OptiScaler proxy name or will use the guarded post-SR fallback. Origin-zero allocation padding is supported on the DX12 pre-SR path and its bridges.

---

<div align="center">

  ![Logo](https://github.com/user-attachments/assets/c7dad5da-0b29-4710-8a57-b58e4e407abd)

</div>
<hr />
<br />
<div align="center">
  <a href="https://github.com/sponsors/cdozdil?frequency=one-time"><img src="images/gh-sponsor-red.png" /></a>
  <a href="https://buymeacoffee.com/nitec"><img src="images/bmac.png" /></a>
</div>
<br />

## Table of Contents

**1.** [**About**](#about)  
**2.** [**How it works?**](#how-it-works)  
**3.** [**Supported APIs and Upscalers**](#which-apis-and-upscalers-are-supported)  
**4.** [**Installation**](#installation)  
**5.** [**Known Issues**](#known-issues)  
**6.** [**Compilation and Credits**](#compilation)  
**7.** [**Wiki**](https://github.com/optiscaler/OptiScaler/wiki)

<br />
<div align="center">
  <a href="https://discord.gg/wEyd9w4hG5"><img src="https://img.shields.io/badge/OptiScaler-blue?style=for-the-badge&logo=discord&logoColor=white&logoSize=auto&color=5865F2" alt="Discord invite"></a>
  <a href="https://github.com/optiscaler/OptiScaler/releases/latest"><img src="https://img.shields.io/badge/Download-Stable-green?style=for-the-badge&logo=github&logoSize=auto" alt="Stable release"></a>
  <a href="https://github.com/optiscaler/OptiScaler/releases/tag/nightly"><img src="https://img.shields.io/badge/Download-Nightly-purple?style=for-the-badge&logo=github&logoSize=auto" alt="Nightly release"></a>
  <a href="https://github.com/optiscaler/OptiScaler/wiki"><img src="https://img.shields.io/badge/Documentation-blue?style=for-the-badge&logo=gitbook&logoColor=white&logoSize=auto" alt="Wiki"></a>
</div>
<div align="center">
  <a href="https://github.com/optiscaler/OptiScaler/releases"><img src="https://img.shields.io/github/downloads/optiscaler/optiscaler/total?style=for-the-badge&logo=gitextensions&logoSize=auto&label=Total" alt="Total DL"></a>
  <a href="https://github.com/optiscaler/OptiScaler/releases/latest"><img src="https://img.shields.io/github/downloads/optiscaler/optiscaler/latest/total?style=for-the-badge&logo=gitextensions&logoSize=auto&label=Stable&color=green&logoColor=white" alt="Stable DL"></a>
  <a href="https://github.com/optiscaler/OptiScaler/releases/tag/nightly"><img src="https://img.shields.io/github/downloads/optiscaler/OptiScaler/nightly/total?style=for-the-badge&logo=gitextensions&logoColor=white&logoSize=auto&label=Nightly&color=purple" alt="Nightly DL"></a>
  <a href="https://github.com/optiscaler/OptiScaler/stargazers"><img src="https://img.shields.io/github/stars/optiscaler/optiscaler?style=for-the-badge&logo=githubsponsors&logoColor=white&label=S.T.A.R.S." alt="Stars"></a>
</div>


## About

**OptiScaler** is a tool that lets you replace upscalers in games that ***already support DLSS2+ / FSR2+ / XeSS*** ($`^1`$), as well as manage ***frame generation*** in already mentioned games _(either by replacing existing FG options or enabling it in DX12 games through experimental ***OptiFG***)_. It also offers extensive customization options for all users, including those with Nvidia GPUs using DLSS.

> [!CAUTION]
> * We've been informed about some **FAKE websites** presenting themselves as OptiScaler team, so we would like to strongly highlight that we **DO NOT HAVE an official website!**  
> * We **DON'T have an official manager app**, so please be careful when downloading or using them! And please don't bother us to provide support for something which isn't even ours!
> * Only **LEGIT places** are this Github, our Discord server and Nitec's NexusMods page.  
> * OptiScaler is **FREE**, any kind of monetary requirements are scams!  

> [!TIP]
> _For example, if a game has DLSS only, OptiScaler can be used to replace DLSS with XeSS or FSR 3.1 (also works for FSR2-only games, like The Outer Worlds Spacer's Choice, albeit requires manually providing nvngx_dlss.dll)._

**Key aspects of OptiScaler:**
- Enables usage of XeSS, FSR2, FSR3, **FSR4**$`^2`$ (_officially, RDNA4 and RDNA3 dGPUs only_) and DLSS in (temporal) upscaler-enabled games
- Allows users to fine-tune their upscaling experience with a wide range of tweaks and enhancements (RCAS & MAS, Output Scaling, DLSS Presets, Ratio & DRS Overrides etc.)
- Since v0.7.0+, added ***experimental DX12*** frame generation support with possible HUDfix solution ([**OptiFG**](#optifg--hudfix-experimental-hud-ghosting-fix))
- Supports [**Fakenvapi**](#installation) integration - enables Reflex hooking and injecting _Anti-Lag 2_ (RDNA1+ only), _LatencyFlex_ (LFX) or _XeLL_ - _bundled since 0.9_  
- Since v0.7.7, added support for **Nukem's** FSR3-FG mod [**dlssg-to-fsr3**](#installation), only supports games with ***native DLSS-FG*** - _bundled since 0.9_
- Since v0.7.8, added **ASI plugin loading** support (_disabled_ by default (`LoadAsiPlugins=` in INI), loads from customisable folder, default `plugins`)
- New project - [**OptiPatcher**](https://github.com/optiscaler/OptiPatcher) - an ASI Plugin for OptiScaler for enabling DLSS and DLSSG inputs without spoofing in ***supported games***.
- Since v0.7.8, OptiScaler is now automatically applying certain game patches for a better out-of-the-box experience
- Since v0.9.0, separated FG Inputs and Outputs, added XeFG and FSR4-FG support, as well as bundled Fakenvapi and Nukem's FSR3-FG mod
- For a detailed list of all features, check [Features](Features.md)


> [!IMPORTANT]
> _**Always check the [Wiki Compatibility list](https://github.com/optiscaler/OptiScaler/wiki) for known game issues and workarounds.**_  
> Also please check the  [***OptiScaler known issues***](#known-issues) at the end regarding **RTSS** compatibility.  
> A separate [***FSR4 Compatibility list***](https://github.com/optiscaler/OptiScaler/wiki/FSR4-Compatibility-List) is available for community-sourced tested games.  
> ***[3]** For **not bundled** items, please check [Installation](#installation).*  

> [!NOTE]
> ### Upscaler notes
> <details>
>  <summary><b>Click for [1], [2] </b></summary>  
>  
> **[1]** For **Unreal Engine** games, only UE XeSS -> Opti XeSS/FSR4 work  
>  
> *Regarding **XeSS** inputs, since **Unreal Engine plugin** does not provide depth, replacing in-game XeSS breaks other upscalers (e.g. Redout 2 as a XeSS-only game), but you can still apply RCAS sharpening to XeSS to reduce blurry visuals.* 
>
> *Regarding **FSR inputs**, FSR 3.1 is the first version with a fully standardised, forward-looking API and should be fully supported. Since FSR2 and FSR3 support custom interfaces, game support will depend on the developers' implementation. With Unreal Engine games, you might need [ini tweaks](https://github.com/optiscaler/OptiScaler/wiki/Unreal-Engine-Tweaks) for FSR inputs.*  
>
> **[2]** *Regarding **FSR4**, please check [FSR4 Compatibility list](https://github.com/optiscaler/OptiScaler/wiki/FSR4-Compatibility-List) for known supported games and general info.*
> 
> </details>


## Official Discord Server: [OptiScaler](https://discord.gg/wEyd9w4hG5)

*This project is based on [PotatoOfDoom](https://github.com/PotatoOfDoom)'s excellent [CyberFSR2](https://github.com/PotatoOfDoom/CyberFSR2).*

## How it works?
* OptiScaler acts as a middleware, it intercepts upscaler calls from the game (_**Inputs**_) and redirects them to the chosen upscaling backend (_**Output**_), allowing user to replace one technology with another one. **Inputs -> OptiScaler -> Outputs**  
* _Or put more bluntly, **Input** is the upscaler used in game settings, and **Output** the one selected in Opti Overlay._
* _Same goes for FG options which are separated into **FG Input** and **FG Output**._

> [!NOTE]
> * Pressing **`Insert`** should open the Optiscaler **Overlay** in-game with all of the options (_`ShortcutKey=` can be changed in the INI file, or under **Keybinds** in the overlay_). 
> * Pressing **`Page Up`** shows the performance stats overlay in the top left, and can be cycled between different modes with **`Page Down`** (_keybinds customisable in the overlay_).  
> * If Opti overlay is instantly disappearing after trying Insert a few times, maybe try **`Alt + Insert`** ([reported workaround](https://github.com/optiscaler/OptiScaler/issues/484) for alternate keyboard layouts).

![inputs_and_outputs](https://github.com/user-attachments/assets/7ff37fd7-515f-488d-99ff-faa586e206fc)

## Which APIs and Upscalers are Supported?
Currently **OptiScaler** can be used with DirectX 11, DirectX 12 and Vulkan, but each API has different sets of supported upscalers.  
[**OptiFG**](#optifg--hudfix-experimental-hud-ghosting-fix) currently **only supports DX12** and is explained in a separate paragraph.

#### For DirectX 12
- XeSS (Default)
- FSR 2.1.2, 2.2.1
- FSR 3.X (and FSR 2.3.X)
- FSR 4.X (via FSR 3.X/4, _officially RDNA4 and RDNA3 dGPUs only_)
- DLSS

#### For DirectX 11
- FSR 2.2.1 (Default, native DX11)
- FSR 3.1.2 (unofficial port to native DX11)
- DLSS (native DX11)
- XeSS 2.X (native DX11, _Intel ARC only_)
- XeSS, FSR 2.1.2, 2.2.1, FSR 3.X w/Dx12 (_via D3D11on12_)$`^1`$
- FSR 4.X (via FSR 3.X/4 w/Dx12 interop, _officially RDNA4 and RDNA3 dGPUs only_)

> [!NOTE]
> <details>
>  <summary><b>Expand for [1]</b></summary>
>
> _**[1]** These implementations use a background DirectX12 device to be able to use DX12-only upscalers. There's a performance penalty up to 10-ish % for this method, but allows many more upscaler options. Also native DX11 implementation of FSR 2.2.1 is a backport from Unity renderer and has its own problems of which some were fixed by OptiScaler._
> </details>

#### For Vulkan
- FSR 4.X (via FSR 3.X/4 w/Dx12 interop, _officially RDNA4 and RDNA3 dGPUs only_)
- FSR2 2.1.2 (Default), 2.2.1
- FSR3 3.1 (and FSR2 2.3.2)
- DLSS
- XeSS 2.x

#### OptiFG + HUDfix (experimental HUD ghosting fix) 
**OptiFG** was added with **v0.7** and is **only supported in DX12**. 
It's an **experimental** way of adding FG to games without native Frame Generation, or can also be used as a last case scenario if the native FG is not working properly.  
* Currently supports FSR3-FG (requires HUDfix to avoid HUD ghosting), XeFG and FSR4-FG (ML model deals with the HUD, so may or may not require HUDfix).

For more information on OptiFG and how to use it, please check the Wiki page - [OptiFG](https://github.com/optiscaler/OptiScaler/wiki/OptiFG).


## Installation
> [!CAUTION]
> _**Warning**: **Do not use this mod with online games.** It may trigger anti-cheat software and cause bans!_

> [!IMPORTANT]
> **For installation steps, please check the [**Wiki**](https://github.com/optiscaler/OptiScaler/wiki)**  

## Configuration
Please check [this](Config.md) document for configuration parameters and explanations. If your GPU is not an Nvidia one, check [GPU spoofing options](Spoofing.md) *(Will be updated)*

## Known Issues

> [!NOTE]
> **For a list of known issues, please check the [**Wiki**](https://github.com/optiscaler/OptiScaler/wiki)**.
> 
> Also worth checking the [Compatibility List](https://github.com/optiscaler/OptiScaler/wiki/Compatibility-List) for possible game issues and their fixes.

## Compilation

### Requirements
* Visual Studio 2022

### Instructions
* Clone this repo with **all of its submodules**.
* Open the OptiScaler.sln with Visual Studio 2022.
* Build the project

## Thanks
* @PotatoOfDoom for CyberFSR2
* @Artur for DLSS Enabler and helping me implement NVNGX api correctly
* @LukeFZ & @Nukem for their great mods and sharing their knowledge 
* @FakeMichau for continous support, testing and feature creep
* @QM for continous testing efforts and helping me to reach games
* @TheRazerMD for continous testing and support
* @Cryio, @krispy, @krisshietala, @Lordubuntu, @scz, @Veeqo for their hard work on (now outdated) [compatibility matrix](https://docs.google.com/spreadsheets/d/1qsvM0uRW-RgAYsOVprDWK2sjCqHnd_1teYAx00_TwUY)
* And the whole DLSS2FSR community for all their support

## Credit
This project uses [FreeType](https://gitlab.freedesktop.org/freetype/freetype) licensed under the [FTL](https://gitlab.freedesktop.org/freetype/freetype/-/blob/master/docs/FTL.TXT)

## Sponsors
<table>
 <tbody>
  <tr>
   <td align="center"><img alt="[SignPath]" src="https://avatars.githubusercontent.com/u/34448643" height="30"/></td>
   <td>Free code signing on Windows provided by <a href="https://signpath.io/">SignPath.io</a>, certificate by <a href="https://signpath.org/">SignPath Foundation</a></td>
  </tr>
 </tbody>
</table>

