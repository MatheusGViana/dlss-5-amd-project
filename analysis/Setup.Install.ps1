param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [ValidateSet('auto','dxgi.dll','winmm.dll','version.dll','winhttp.dll','wininet.dll','dbghelp.dll')]
    [string]$ProxyName='auto'
)
$ErrorActionPreference='Stop'
# Check the complete backend before changing any game files. These binaries
# expose a private ABI and cannot be mixed with another OptiScaler AMD release.
$runtimeHash='3C9CA13F0F5FC36A690BA424C457003BCFCC1080B4B785974CDD7E9AE2BC1DD8'
foreach ($pass in 1..3) {
    $runtime=Join-Path $PSScriptRoot "dlssnr_amd_pass$pass.dll"
    if (!(Test-Path -LiteralPath $runtime -PathType Leaf) -or (Get-FileHash -LiteralPath $runtime).Hash -ne $runtimeHash) {
        throw "Missing or incompatible AMD pass $pass. Extract the complete release package before running Setup."
    }
}
foreach ($required in @('OptiScaler.dll','OptiScaler.ini','dlssnr_on_amd_weights.bin','OptiScaler')) {
    if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot $required))) {throw "Incomplete package: $required is missing."}
}
foreach ($required in @('GatherCS.cso','ResolveCS.cso')) {
    if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot ('experimental_lighting/'+$required)) -PathType Leaf)) {throw "Incomplete RTGI package: $required is missing."}
}
$game=(Resolve-Path -LiteralPath $GameDir).Path
if (!(Test-Path -LiteralPath $game -PathType Container)) {throw 'Informe a pasta do executavel do jogo.'}
$running=Get-Process -ErrorAction SilentlyContinue | Where-Object {try {$_.Path -and ([IO.Path]::GetDirectoryName($_.Path) -eq $game)} catch {$false}}
if ($running) {throw 'Feche o jogo antes de instalar.'}
$proxies=@('dxgi.dll','winmm.dll','version.dll','winhttp.dll','wininet.dll','dbghelp.dll') | ForEach-Object {
    $candidate=Join-Path $game $_
    if(Test-Path -LiteralPath $candidate -PathType Leaf) {
        $item=Get-Item -LiteralPath $candidate
        if($item.VersionInfo.ProductName -eq 'OptiScaler' -or $item.VersionInfo.FileDescription -eq 'OptiScaler') {$item.Name}
    }
}
if(@($proxies).Count -gt 1){throw ('Mais de um proxy OptiScaler encontrado: '+($proxies -join ', ')+'. Mantenha apenas o proxy que deseja usar antes de atualizar.')}
$proxyName=if($ProxyName -eq 'auto') {
    if(@($proxies).Count -eq 1){@($proxies)[0]}else{'dxgi.dll'}
} else {$ProxyName.ToLowerInvariant()}
if(@($proxies).Count -eq 1 -and $ProxyName -ne 'auto' -and @($proxies)[0] -ne $proxyName) {
    throw ('OptiScaler is already installed as '+@($proxies)[0]+'. Move it before selecting '+$proxyName+'.')
}
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
if((Test-Path -LiteralPath $standalone) -and $proxyName -ne 'version.dll') {
    $sha=(Get-FileHash -LiteralPath $standalone).Hash
    if($sha -ne '106223723FD9266C44D38DC2FB77933948AB37803F46BFCEA2BAE3A0A474AC84') {
        throw 'Existe uma version.dll diferente da original analisada. Identifique-a antes de instalar para evitar conflito de proxies.'
    }
    Move-Item -LiteralPath $standalone -Destination (Join-Path $backup 'version.dll')
}
Install-File (Join-Path $PSScriptRoot 'OptiScaler.dll') $proxyName
foreach($name in @('OptiScaler.ini','dlssnr_amd_pass1.dll','dlssnr_amd_pass2.dll','dlssnr_amd_pass3.dll','dlssnr_on_amd_weights.bin')) {
    Install-File (Join-Path $PSScriptRoot $name) $name
}
$deps=Join-Path $PSScriptRoot 'OptiScaler'
Get-ChildItem -LiteralPath $deps -Recurse -File | ForEach-Object {
    $relative='OptiScaler\'+$_.FullName.Substring($deps.Length).TrimStart('\')
    Install-File $_.FullName $relative
}
$rtgi=Join-Path $PSScriptRoot 'experimental_lighting'
Get-ChildItem -LiteralPath $rtgi -File | ForEach-Object {
    Install-File $_.FullName ('experimental_lighting\'+$_.Name)
}
$records | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $backup 'manifest.json')
Write-Host "Instalado em: $game"
Write-Host "Proxy: $proxyName"
Write-Host "Backup em: $backup"
Write-Host 'Ative FSR no jogo. Abra o menu do OptiScaler com Insert. Comece com uma passagem.'
