param([string]$PackageName='OptiScaler-AMD-PreSR-Multipass-v1.2')
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'OptiScaler-DLSSNR-PreSR-Multipass-main'
$stage=Join-Path $PSScriptRoot $PackageName
New-Item -ItemType Directory -Force -Path $stage,(Join-Path $stage 'OptiScaler'),(Join-Path $stage 'Licenses') | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'x64/Release/OptiScaler.dll') -Destination $stage -Force
foreach($name in @('dlssnr_amd_pass1.dll','dlssnr_amd_pass2.dll','dlssnr_amd_pass3.dll','dlssnr_on_amd_weights.bin')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "package-amd-presr/$name") -Destination $stage -Force
}
$deps=Join-Path $stage 'OptiScaler'
foreach($name in @('amd_fidelityfx_loader_dx12.dll','amd_fidelityfx_upscaler_dx12.dll','amd_fidelityfx_framegeneration_dx12.dll')) {
    Copy-Item -LiteralPath (Join-Path $source "external/FidelityFX-SDK-v2/Kits/FidelityFX/signedbin/$name") -Destination $deps -Force
}
Copy-Item -LiteralPath (Join-Path $source 'external/FidelityFX-SDK/PrebuiltSignedDLL/amd_fidelityfx_vk.dll') -Destination $deps -Force
Get-ChildItem -LiteralPath (Join-Path $source 'external/xess/bin') -Filter '*.dll' | Copy-Item -Destination $deps -Force
New-Item -ItemType Directory -Path (Join-Path $deps 'D3D12_OptiScaler') -Force | Out-Null
Get-ChildItem -LiteralPath (Join-Path $source 'external/directx_agility_sdk/lib') -Filter '*.dll' | Copy-Item -Destination (Join-Path $deps 'D3D12_OptiScaler') -Force
Copy-Item -LiteralPath (Join-Path $source 'LICENSE') -Destination (Join-Path $stage 'Licenses/OptiScaler_LICENSE.txt') -Force
Get-ChildItem -LiteralPath (Join-Path $source 'Licenses') -File | Copy-Item -Destination (Join-Path $stage 'Licenses') -Force
foreach($entry in @(@('external/xess/LICENSE.txt','XeSS_LICENSE.txt'),@('external/FidelityFX-SDK/docs/license.md','FidelityFX_v1_LICENSE.md'),@('external/FidelityFX-SDK-v2/docs/license.md','FidelityFX_v2_LICENSE.md'),@('external/directx_agility_sdk/LICENSE.txt','DirectX_LICENSE.txt'))) {
    Copy-Item -LiteralPath (Join-Path $source $entry[0]) -Destination (Join-Path $stage ('Licenses/'+$entry[1])) -Force
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'analysis/INSTALAR_AMD.ps1'),(Join-Path $PSScriptRoot 'analysis/LEIA-ME-AMD.md') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'analysis/DIAGNOSTICO_AMD.ps1') -Destination $stage -Force
$ini=Get-Content -LiteralPath (Join-Path $source 'OptiScaler.ini') -Raw
$ini=$ini -replace '(?m)^Dx12Upscaler=.*$','Dx12Upscaler=ffx'
$ini=$ini -replace '(?m)^LogToFile=.*$','LogToFile=true'
$ini=$ini -replace '(?m)^LogLevel=.*$','LogLevel=2'
$ini=[regex]::Replace($ini,'(?ms)(\[FrameGen\].*?^Enabled=)[^\r\n]*','$1false')
$ini=[regex]::Replace($ini,'(?ms)^\[DlssNr\].*?(?=^\[|\z)',@'
[DlssNr]
; AMD HIP backend: active-resolution pre-SR, independent runtime per pass.
Enabled=true
RunBeforeSR=true
Passes=1
LocalTone=0
LocalStructure=1
SkinStructure=1
ApplyAfterRR=false

'@)
[IO.File]::WriteAllText((Join-Path $stage 'OptiScaler.ini'),$ini,[Text.UTF8Encoding]::new($false))
$hashes=Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object {$_.Name -ne 'SHA256SUMS.txt'} | Sort-Object FullName | ForEach-Object {
    '{0} *{1}' -f (Get-FileHash -LiteralPath $_.FullName).Hash,$_.FullName.Substring($stage.Length+1)
}
$hashes | Set-Content -LiteralPath (Join-Path $stage 'SHA256SUMS.txt')
$zip=Join-Path $PSScriptRoot ($PackageName+'.zip')
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal -Force
Write-Output $zip
