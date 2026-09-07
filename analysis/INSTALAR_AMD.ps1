param([Parameter(Mandatory=$true)][string]$GameDir)
$ErrorActionPreference='Stop'
$game=(Resolve-Path -LiteralPath $GameDir).Path
if (!(Test-Path -LiteralPath $game -PathType Container)) {throw 'Informe a pasta do executavel do jogo.'}
$running=Get-Process -ErrorAction SilentlyContinue | Where-Object {try {$_.Path -and ([IO.Path]::GetDirectoryName($_.Path) -eq $game)} catch {$false}}
if ($running) {throw 'Feche o jogo antes de instalar.'}
$backup=Join-Path $game ('backup-amd-presr-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
$records=[Collections.Generic.List[object]]::new()
function Install-File([string]$source,[string]$relative) {
    $dest=[IO.Path]::GetFullPath((Join-Path $game $relative))
    if (!$dest.StartsWith($game.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Destino fora da pasta do jogo.'}
    $existed=Test-Path -LiteralPath $dest
    if ($existed) {
        $saved=Join-Path $backup $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
        Copy-Item -LiteralPath $dest -Destination $saved
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $dest -Force
    $records.Add([pscustomobject]@{File=$relative;Existed=$existed;InstalledSHA256=(Get-FileHash -LiteralPath $dest).Hash})
}
# The original AMD proxy would otherwise evaluate NR a second time after FSR.
$standalone=Join-Path $game 'version.dll'
if(Test-Path -LiteralPath $standalone) {
    $sha=(Get-FileHash -LiteralPath $standalone).Hash
    if($sha -ne '106223723FD9266C44D38DC2FB77933948AB37803F46BFCEA2BAE3A0A474AC84') {
        throw 'Existe uma version.dll diferente da original analisada. Identifique-a antes de instalar para evitar conflito de proxies.'
    }
    Move-Item -LiteralPath $standalone -Destination (Join-Path $backup 'version.dll')
}
Install-File (Join-Path $PSScriptRoot 'OptiScaler.dll') 'dxgi.dll'
foreach($name in @('OptiScaler.ini','dlssnr_amd_pass1.dll','dlssnr_amd_pass2.dll','dlssnr_amd_pass3.dll','dlssnr_on_amd_weights.bin')) {
    Install-File (Join-Path $PSScriptRoot $name) $name
}
$deps=Join-Path $PSScriptRoot 'OptiScaler'
Get-ChildItem -LiteralPath $deps -Recurse -File | ForEach-Object {
    $relative='OptiScaler\'+$_.FullName.Substring($deps.Length).TrimStart('\')
    Install-File $_.FullName $relative
}
$records | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $backup 'manifest.json')
Write-Host "Instalado em: $game"
Write-Host "Backup em: $backup"
Write-Host 'Ative FSR no jogo. Abra o menu do OptiScaler com Insert. Comece com uma passagem.'
