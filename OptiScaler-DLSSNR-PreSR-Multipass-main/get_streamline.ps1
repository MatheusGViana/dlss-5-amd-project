# Fetch the unmodified production DLLs for OptiScaler's own DLSS Frame Generation output.
# Never enables FG, changes an INI, or replaces an existing different Streamline stack.
# Requires Windows PowerShell 5.1+ (Authenticode verification needs Windows).
param(
    [Parameter(Mandatory = $true)]
    [string]$Destination,
    [string]$ArchivePath,
    [switch]$AcceptNvidiaLicenses
)

$ErrorActionPreference = 'Stop'
if (-not $AcceptNvidiaLicenses) {
    throw 'Read docs/DLSS-FRAME-GENERATION.md and the linked NVIDIA licences, then pass -AcceptNvidiaLicenses if you agree. Nothing was installed.'
}
if (-not (Get-Command Get-AuthenticodeSignature -ErrorAction SilentlyContinue)) {
    throw 'Run this on Windows: NVIDIA Authenticode signatures must be verified.'
}

$manifestPath = Join-Path $PSScriptRoot 'redist\streamline\manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$destinationPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Destination)

function Assert-PayloadFile($Path, $Record) {
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Record.sha256) {
        throw "Hash mismatch: $Path. Not replacing this file; back up and move an old stack aside yourself."
    }
    if ($Record.name.EndsWith('.dll', [StringComparison]::OrdinalIgnoreCase)) {
        $signature = Get-AuthenticodeSignature -LiteralPath $Path
        if ($signature.Status -ne 'Valid' -or
            $signature.SignerCertificate.Thumbprint -ne $Record.signerThumbprint) {
            throw "NVIDIA signature verification failed: $Path ($($signature.Status)). Do not disable signature checking or antivirus."
        }
    }
}

# Preflight the whole destination before downloading or copying anything. The destination must be
# OptiScaler's dedicated plugin folder, not a game's executable directory or its native SL folder.
if ((Split-Path -Leaf $destinationPath) -ne 'streamline') {
    throw 'Destination must be the dedicated streamline subfolder, normally GAME\OptiScaler\streamline.'
}
if (Test-Path -LiteralPath $destinationPath) {
    foreach ($existing in Get-ChildItem -LiteralPath $destinationPath -Force) {
        $record = $manifest.files | Where-Object { $_.name -eq $existing.Name }
        if ($existing.PSIsContainer -or -not $record -or
            ($existing.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Unexpected file or directory in destination: $($existing.FullName). Use a clean dedicated folder."
        }
        Assert-PayloadFile $existing.FullName $record
    }
}

$cachePath = Join-Path $PSScriptRoot ".dependencies\streamline\$($manifest.version)"
New-Item -ItemType Directory -Path $cachePath -Force | Out-Null
if (-not $ArchivePath) {
    $ArchivePath = Join-Path $cachePath 'streamline-sdk.zip'
    if (-not (Test-Path -LiteralPath $ArchivePath)) {
        Write-Host "Downloading NVIDIA Streamline $($manifest.version) from $($manifest.archiveUrl)"
        # Use a separate file so an interrupted download cannot become a supposedly complete cache.
        $downloadPath = Join-Path $cachePath ("download-{0}.zip" -f [Guid]::NewGuid().ToString('N'))
        Invoke-WebRequest -Uri $manifest.archiveUrl -OutFile $downloadPath -UseBasicParsing
        if ((Get-FileHash -LiteralPath $downloadPath -Algorithm SHA256).Hash -ne $manifest.archiveSha256) {
            throw "Official archive checksum mismatch. Untrusted download retained for inspection at $downloadPath; nothing installed."
        }
        Move-Item -LiteralPath $downloadPath -Destination $ArchivePath
    }
}
$ArchivePath = (Resolve-Path -LiteralPath $ArchivePath).ProviderPath
if ((Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash -ne $manifest.archiveSha256) {
    throw "Wrong or damaged Streamline archive: $ArchivePath. Expected SHA-256 $($manifest.archiveSha256)."
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$payloadPath = Join-Path $cachePath 'production-fg'
New-Item -ItemType Directory -Path $payloadPath -Force | Out-Null
$sdkZip = [IO.Compression.ZipFile]::OpenRead($ArchivePath)
try {
    foreach ($record in $manifest.files) {
        $payloadFile = Join-Path $payloadPath $record.name
        if (-not (Test-Path -LiteralPath $payloadFile)) {
            $entry = $sdkZip.GetEntry($record.entry)
            if (-not $entry) { throw "Missing required SDK file: $($record.entry)" }
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $payloadFile)
        }
        Assert-PayloadFile $payloadFile $record
    }
}
finally {
    $sdkZip.Dispose()
}

# All files have now passed their checks. Copy only the allowlist, never the SDK directory wholesale.
New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
foreach ($record in $manifest.files) {
    $targetPath = Join-Path $destinationPath $record.name
    if (-not (Test-Path -LiteralPath $targetPath)) {
        [IO.File]::Copy((Join-Path $payloadPath $record.name), $targetPath, $false)
    }
    Assert-PayloadFile $targetPath $record
}
Write-Host "Verified six NVIDIA-signed production DLLs and four licence/notice files in $destinationPath"
Write-Host 'Frame generation remains unchanged. See docs/DLSS-FRAME-GENERATION.md to select the correct FG owner.'
