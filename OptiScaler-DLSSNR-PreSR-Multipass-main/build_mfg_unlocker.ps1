# Optional source-only integration of dashdogy/RTX40MFG-Unlock.
# Does not download binaries, install into a game, or change game settings.
[CmdletBinding()]
param(
    [string]$CMake = 'cmake',
    [string]$ReShadeRoot = '',
    [string]$ImGuiRoot = ''
)
$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot
$source = Join-Path $repoRoot 'external/RTX40MFG-Unlock'
$pinned = '4e776d068f91b4a665425542bb005dd57cc3d891'
$actual = & git -C $source rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $pinned) {
    throw 'Missing/wrong unlocker revision. Run: git submodule update --init external/RTX40MFG-Unlock'
}
if (& git -C $source status --porcelain) { throw 'Unlocker source is modified; refusing an unpinned build.' }
$build = Join-Path $repoRoot 'x64/mfg-unlock-build'
$sdk = Join-Path $build 'sdk'
# Reuse the SDK headers already tracked by OptiScaler, in the layout upstream CMake expects.
New-Item -ItemType Directory -Path "$sdk/include", "$sdk/external/ngx-sdk/include" -Force | Out-Null
Copy-Item "$repoRoot/external/streamline/*.h" "$sdk/include" -Force
Copy-Item "$repoRoot/external/nvngx_dlss_sdk/*.h" "$sdk/external/ngx-sdk/include" -Force
$withUI = $ReShadeRoot -ne '' -and $ImGuiRoot -ne ''
if (($ReShadeRoot -ne '') -ne ($ImGuiRoot -ne '')) { throw 'Supply both ReShadeRoot and ImGuiRoot, or neither.' }
$ui = if ($withUI) { 'ON' } else { 'OFF' }
& $CMake -S "$source/source/native" -B $build -G 'Visual Studio 17 2022' -A x64 `
    "-DSTREAMLINE_ROOT=$sdk" "-DMFG_UNLOCK_BUILD_UNIVERSAL_UI=$ui" `
    "-DRESHADE_ROOT=$ReShadeRoot" "-DIMGUI_ROOT=$ImGuiRoot"
if ($LASTEXITCODE -ne 0) { throw 'Unlocker CMake configuration failed' }
$targets = @('RTX40MFGCore', 'RTX40MFGAuto')
if ($withUI) { $targets += 'RTX40MFGReShadeUI' }
& $CMake --build $build --config Release --parallel 4 --target $targets
if ($LASTEXITCODE -ne 0) { throw 'Unlocker build failed' }
# New output per build avoids leaving a stale UI add-on in a core-only package.
$out = Join-Path $repoRoot ('release/mfg-optional-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $out | Out-Null
Copy-Item "$build/Release/RTX40MFGCore.dll", "$build/Release/RTX40MFG.asi" $out
if ($withUI) { Copy-Item "$build/Release/RTX40MFG-UI.addon64" $out }
Copy-Item "$source/LICENSE" "$out/RTX40MFG-LICENSE.txt"
Copy-Item "$source/source/native/third_party/minhook/LICENSE.txt" "$out/MinHook-LICENSE.txt"
Copy-Item "$source/README.md" "$out/UPSTREAM-README.md"
Copy-Item "$repoRoot/docs/RTX40-MFG.md" "$out/INSTALL.md"
Write-Output "Optional unlocker built from $pinned at $out"
Get-ChildItem $out -File | Get-FileHash -Algorithm SHA256 | Format-Table -AutoSize
