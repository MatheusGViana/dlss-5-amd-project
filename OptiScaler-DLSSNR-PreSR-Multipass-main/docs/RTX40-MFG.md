# RTX 40 MFG unlocker (optional)

This fork can leave frame generation to [Dashdogy's RTX40MFG-Unlock](https://github.com/dashdogy/RTX40MFG-Unlock).
The unlocker is separate, MIT-licensed work by Michael Robles. Its source is pinned as a git submodule
at `4e776d068f91b4a665425542bb005dd57cc3d891`. No NVIDIA FG binary is bundled or patched on disk by us.
This integration has been built, but has not been verified on RTX 40 hardware. It is not an RTX 30
FG unlocker and does not add native DLSS FG to games which lack it.

## Use with OptiScaler

1. Back up your working setup. Close the game.
2. Install the unlocker using its own README. Keep its loader separate from OptiScaler and ReShade.
   Do not overwrite an existing `dxgi.dll`, `version.dll`, `dinput8.dll`, or other proxy owned by a mod.
   The ASI must load before the first FG pipeline; use the upstream-recommended early ASI loader.
   Do not rely on OptiScaler's late ASI-plugin loading for this.
3. In `OptiScaler.ini`, set `[FrameGen] External=true`. Or select **External frame generation / MFG
   unlocker** in the overlay, Save Settings, then restart.
4. Enable native DLSS FG in the game. Select the multiplier in the unlocker/game, not OptiScaler.
5. Start at 2x, then try 3x. Confirm the active provider and applied multiplier in the unlocker log,
   not just the requested setting or an FPS counter.

External mode disables OptiScaler's Streamline interception, NVIDIA API overrides (including Reflex,
flip metering and driver-preset interception), multiplier overrides and replacement FG routing.
Streamline/FG DLL loads also pass through to the original loader. NR and NGX upscaling remain available.
It is a startup option: switching ownership without restarting is unsafe. Your saved OptiScaler FG
settings are retained and return on a later startup with `External=false`.
Use the game's/driver's FPS limiter in this mode; OptiScaler does not own Reflex pacing.
Do not use this mode when you need OptiScaler to replace DLSSG with FSR FG on an RTX 30 card.

Keep the game's working Streamline and NVIDIA runtime DLLs. Do not copy a second Streamline stack
from another game's NR/FG package. Disable competing NR injectors when testing this fork.

For a frozen image above 2x, upstream documents FG Preset B as a reported workaround in some games;
Cyberpunk recovery was not separately confirmed. See the upstream README for current limitations.
No security exclusions or disabled antivirus are required. Use single-player games without anti-cheat.

## Build the optional unlocker

Install VS 2022 C++ Build Tools and CMake 3.24+. From this repository:

```powershell
git submodule update --init external/RTX40MFG-Unlock
.\build_mfg_unlocker.ps1
```

If CMake isn't on PATH, pass `-CMake 'C:\path\to\cmake.exe'`.
This builds the core DLL and ASI into a separate `release/mfg-optional-*` folder. It does not install
anything into a game or include the unlocker in normal OptiScaler releases.
The core defaults to following the game. To build the optional ReShade control panel too, provide
`-ReShadeRoot` and `-ImGuiRoot` pointing to matching source trees as described by upstream.
Without the panel, use the game's multiplier or the upstream JSON configuration mechanism.
For example, with the game closed, merge these keys into `RTX40MFG-Universal.json` beside the real
game executable (preserve any other keys):

```json
{"followGame": false, "mode": "fixed", "multiplier": 3, "dynamicTargetFrameRate": 0, "dynamicExperimental56": false}
```

If the legacy CET `plugins/cyber_engine_tweaks/mods/RTX40MFG/init.lua` exists, the universal JSON
lives in that mod's folder instead. `RTX40_MFG_CONFIG_PATH`, if set, overrides both locations.
The unlocker may limit the request to a supported multiplier; inspect its log.

For a report, include the GPU, driver, game and unlocker versions, loader filenames, `OptiScaler.ini`,
`OptiScaler.log` and `%TEMP%\MfgUnlock-<PID>.log`. Remove personal paths before posting publicly.
