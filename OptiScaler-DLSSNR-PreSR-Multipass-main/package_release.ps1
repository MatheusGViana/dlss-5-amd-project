# Assemble a release zip.
#
# The build output directory is not the release: it also holds import libraries, export files, debug
# symbols, and whatever earlier experiments left behind. Shipping that folder wholesale is how a
# release ends up containing a DLL nobody meant to publish, so this copies an explicit list and
# refuses anything not on it.
#
# What is deliberately NOT here: nvngx_dlssnr.dll. That is NVIDIA's, it is not ours to redistribute,
# and the user supplies their own copy per game folder. Only the ~108 KB forwarder ships.

param(
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]*$')]
    [string]$Version = "v0.1.0-dlssnr",
    [switch]$SkipBuild,
    [switch]$IncludeDlssFrameGeneration,
    [switch]$AcceptNvidiaLicenses,
    [string]$StreamlineArchive
)

$ErrorActionPreference = "Stop"

# Derived rather than hardcoded, so this packages whichever checkout it is sitting in. There is more
# than one now -- the experiment runs in a git worktree beside the main tree, and a hardcoded root
# silently packages the other one's build output while reporting success.
$root = Split-Path -Parent $PSCommandPath
$flavour = if ($IncludeDlssFrameGeneration) { '-with-dlss-fg' } else { '' }
$stage = "$root\release\$Version$flavour"
$zip = "$root\release\OptiScaler-DLSSNR-$Version$flavour.zip"
if ((Test-Path -LiteralPath $stage) -or (Test-Path -LiteralPath $zip)) {
    throw 'Release output already exists. Choose a new -Version; existing packages are never deleted or overwritten.'
}
if ($IncludeDlssFrameGeneration -and -not $AcceptNvidiaLicenses) {
    throw 'Bundling NVIDIA binaries requires -AcceptNvidiaLicenses. Read docs/DLSS-FRAME-GENERATION.md first.'
}
if ($IncludeDlssFrameGeneration) {
    Write-Warning 'LOCAL USE ONLY: this DLL-containing package has not been cleared for redistribution. Publish the downloader-only variant instead; see docs/DLSS-FRAME-GENERATION.md.'
}

if (-not $SkipBuild) {
    $msb = (Get-Command MSBuild.exe -ErrorAction SilentlyContinue).Source
    if (-not $msb) {
        $msb = @(
            "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if (-not $msb) {
        throw 'MSBuild.exe was not found. Install Visual Studio C++ build tools or use -SkipBuild with a verified existing build.'
    }

    foreach ($proj in @("$root\OptiScaler\dlssnr\forwarder\dlssnr_forwarder.vcxproj", "$root\OptiScaler.sln")) {
        $out = & $msb $proj /p:Configuration=Release /p:Platform=x64 /v:minimal /m 2>&1
        $err = $out | Select-String "error "
        if ($err) { Write-Host "FAILED: $proj"; $err | Select-Object -First 6; exit 1 }
    }
    Write-Host "built"
}

$src = "$root\x64\Release\a"

# The forwarder is taken from its own build output, not from the shared folder. The solution build
# does not reliably rebuild it, and a stale one here would ship silently.
#
# A fresh checkout has no per-project output directory until that project has been built on its own,
# so fall back to the shared folder rather than failing. The export check below is what actually
# guards against a stale one, and it runs either way.
$forwarder = "$root\OptiScaler\dlssnr\forwarder\x64\Release\a\nvngx.dll_dlssnr.dll"

if (-not (Test-Path $forwarder)) {
    $forwarder = "$root\x64\Release\a\nvngx.dll_dlssnr.dll"
    Write-Host "forwarder: using the shared build output ($forwarder)"
}

$exports = @("dlssnr_call_create", "dlssnr_call_evaluate", "dlssnr_call_set_extras")
$bytes = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($forwarder))
$missing = @($exports | Where-Object { $bytes.IndexOf($_) -lt 0 })

if ($missing.Count -gt 0) {
    Write-Host "STALE forwarder: missing $($missing -join ', ')"
    exit 1
}

New-Item -ItemType Directory -Force -Path $stage | Out-Null

# Files, then folders. Anything not named here does not ship. Runtime files and user-facing
# instructions come from the checkout rather than the build directory so a stale post-build copy
# cannot put old GPU guidance or an old INI into a fresh package.
$buildFiles = @(
    "OptiScaler.dll",
    "!! EXTRACT ALL FILES TO GAME FOLDER !!"
)

$sourceFiles = @(
    "OptiScaler.ini",
    "setup_windows.bat",
    "setup_linux.sh",
    "get_streamline.ps1",
    "README.md",
    "INSTALL-DLSSNR.md",
    "LICENSE"
)

foreach ($f in $buildFiles) {
    $source = "$src\$f"
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Required build output is missing: $source"
    }
    Copy-Item -LiteralPath $source -Destination "$stage\$f" -Force
}

foreach ($f in $sourceFiles) {
    $source = "$root\$f"
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Required release file is missing: $source"
    }
    Copy-Item -LiteralPath $source -Destination "$stage\$f" -Force
}

foreach ($d in @("Licenses", "OptiScaler")) {
    $source = "$src\$d"
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        throw "Required dependency directory is missing: $source"
    }
    Copy-Item -LiteralPath $source -Destination "$stage\$d" -Recurse -Force
}

Copy-Item $forwarder "$stage\nvngx.dll_dlssnr.dll" -Force
Copy-Item -LiteralPath "$root\docs" -Destination "$stage\docs" -Recurse -Force
New-Item -ItemType Directory -Path "$stage\redist\streamline" -Force | Out-Null
Copy-Item -LiteralPath "$root\redist\streamline\manifest.json" -Destination "$stage\redist\streamline\manifest.json"

# An old test stack in the build directory must not silently enter a normal release. The optional
# full application package gets only the pinned official production files, with their own licences.
if (Test-Path -LiteralPath "$stage\OptiScaler\streamline") {
    throw 'REFUSING: the build output contains an unmanaged Streamline stack. Move it aside and use -IncludeDlssFrameGeneration.'
}
if ($IncludeDlssFrameGeneration) {
    & "$root\get_streamline.ps1" -Destination "$stage\OptiScaler\streamline" `
        -ArchivePath $StreamlineArchive -AcceptNvidiaLicenses
}

# Logging on, in the release only.
#
# Upstream ships LogToFile=auto, which resolves to false, and the source ini is theirs -- changing it
# in the repo would put a log-behaviour change into a PR that is about neural rendering. But this is
# an experimental build whose notes ask people to attach OptiScaler.log, and the first release shipped
# asking for a file that was never written.
#
# Info rather than Trace: every line explaining why the pass did not start is Info or worse, so it
# answers the common report at almost no cost. Crash reports need Trace and synchronous writes, and
# the notes say so rather than everyone paying for it.
$iniPath = "$stage\OptiScaler.ini"
$ini = Get-Content $iniPath -Raw
$ini = $ini -replace '(?m)^LogToFile=auto', 'LogToFile=true'
$ini = $ini -replace '(?m)^LogLevel=auto', 'LogLevel=2'
Set-Content $iniPath $ini -Encoding utf8 -NoNewline

$check = Select-String -Path $iniPath -Pattern '^LogToFile=|^LogLevel=' | ForEach-Object { $_.Line }
Write-Host "log settings: $($check -join ', ')"

$targetProcess = Select-String -Path $iniPath -Pattern '^TargetProcessName=' | Select-Object -First 1
if ($targetProcess.Line -ne 'TargetProcessName=auto') {
    throw "REFUSING: portable package has a game-specific process filter: $($targetProcess.Line)"
}
Write-Host "process filter: portable (TargetProcessName=auto)"

# Belt and braces: nothing that is a build artifact, and nothing from the abandoned warp work, may
# survive into the zip regardless of how it got into the staging folder.
Get-ChildItem $stage -Recurse -Include *.exp, *.lib, *.pdb, *.ilk, *latewarp* | Remove-Item -Force

# No feature may ship switched on by accident.
#
# A global regex on "^Enabled=auto" once turned on five sections at once -- output scaling,
# sharpening, the magnifier and two more -- while trying to enable one, because the ini has six keys
# called Enabled in six different sections. That was in a test install rather than a release, and
# only because nothing was checking. This checks.
$on = Select-String -Path "$stage\OptiScaler.ini" -Pattern '^Enabled=true'

if ($on) {
    Write-Host "REFUSING: the packaged ini has features switched on:"
    $on | ForEach-Object { "  line $($_.LineNumber): $($_.Line)" }
    exit 1
}

Write-Host "ini verified: nothing switched on by default"

# The proprietary runtime must never slip into a public artifact. Its two approved hashes are
# documentation/diagnostic inputs only; users obtain the GPU-appropriate file themselves.
if (Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Name -ieq 'nvngx_dlssnr.dll' }) {
    throw 'REFUSING: proprietary nvngx_dlssnr.dll is present in the staging directory'
}

$crossGenHash = 'E67DEE209320CDAFE0E93E45675D7AA34323A53ACC57A72B2E40A181581C989A'
foreach ($requiredTextFile in @("$stage\README.md", "$stage\INSTALL-DLSSNR.md", "$stage\setup_windows.bat")) {
    if ((Get-Content -LiteralPath $requiredTextFile -Raw).IndexOf($crossGenHash, [StringComparison]::OrdinalIgnoreCase) -lt 0) {
        throw "REFUSING: cross-generation runtime hash is missing from $requiredTextFile"
    }
}
Write-Host "cross-generation guidance: present and hash-pinned"

# Hash every shipped file after the staging tree is final. Use forward slashes so the list is easy
# to verify from PowerShell, 7-Zip, Linux, or Wine.
$checksumLines = Get-ChildItem -LiteralPath $stage -Recurse -File |
    Where-Object { $_.Name -ne 'SHA256SUMS.txt' } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($stage, $_.FullName).Replace('\', '/')
        "{0} *{1}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash, $relative
    }
[IO.File]::WriteAllLines("$stage\SHA256SUMS.txt", $checksumLines, [Text.UTF8Encoding]::new($false))
Write-Host "checksums: $($checksumLines.Count) files"

Compress-Archive -Path "$stage\*" -DestinationPath $zip -CompressionLevel Optimal

Write-Host ""
Write-Host "staged at $stage"
Get-ChildItem $stage | ForEach-Object { "  {0,-42} {1,10:N0}" -f $_.Name, $_.Length }
Write-Host ""
Write-Host ("zip: {0}  ({1:N1} MB)" -f $zip, ((Get-Item $zip).Length / 1MB))
