param([string]$PackageName='OptiScaler-AMD-PreSR-Multipass-v2.25')
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
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'analysis/Setup.Install.ps1'),(Join-Path $PSScriptRoot 'analysis/LEIA-ME-AMD.md') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'analysis/Setup.bat'),(Join-Path $PSScriptRoot 'analysis/Setup.GUI.ps1') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'analysis/README-v225.md') -Destination (Join-Path $stage 'README.md') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'analysis/README-v225.md') -Destination (Join-Path $stage 'LEIA-ME-AMD.md') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'DLSS5_Look.fx') -Destination (Join-Path $stage 'Licenses/DLSS5_Look_reference.fx') -Force
$ini=Get-Content -LiteralPath (Join-Path $source 'OptiScaler.ini') -Raw
$ini=$ini -replace '(?m)^Dx12Upscaler=.*$','Dx12Upscaler=ffx'
$ini=$ini -replace '(?m)^LogToFile=.*$','LogToFile=true'
$ini=$ini -replace '(?m)^LogLevel=.*$','LogLevel=2'
$ini=[regex]::Replace($ini,'(?ms)(\[FrameGen\].*?^Enabled=)[^\r\n]*','$1false')
$ini=[regex]::Replace($ini,'(?ms)^\[DlssNr\].*?(?=^\[|\z)',@'
[DlssNr]
; AMD HIP backend: active-resolution pre-SR, independent runtime per pass.
Enabled=false
RunBeforeSR=true
AmdModelScale=1
AmdEncoding=0
AmdNeuralLighting=true
AmdNeuralLightingStrength=0.5
Passes=1
LocalTone=0
LocalStructure=1
SkinStructure=1
ApplyAfterRR=false

'@)
[IO.File]::WriteAllText((Join-Path $stage 'OptiScaler.ini'),$ini,[Text.UTF8Encoding]::new($false))
$lookConfig=@'

[AmdLook]
; Built-in spatial appearance filter. Requires the AMD neural/pre-SR path.
; No ReShade required. This does not restore disabled neural lighting channels.
Enabled=false
Appearance=2
Mix=1
MaterialDetail=1.15
ShapeDefinition=1.2
LocalLighting=1.15
SkinDetail=1.1
SkinSoftness=0.486
DetectSkin=true
SpecularControl=0.58
HighlightRollOff=0.9
ColourSeparation=0
ShadowDepth=0.2
AntiHalo=0.901
FlatAreaProtection=0
Inspect=0
Tone=0
ExposureEV=1
Contrast=1
Saturation=1
HighlightCompression=0
'@
Add-Content -LiteralPath (Join-Path $stage 'OptiScaler.ini') -Value $lookConfig
$rtgiSource=Join-Path $PSScriptRoot 'package-amd-presr/experimental_lighting'
$rtgiTarget=Join-Path $stage 'experimental_lighting'
New-Item -ItemType Directory -Path $rtgiTarget -Force | Out-Null
Get-ChildItem -LiteralPath $rtgiSource -File | Copy-Item -Destination $rtgiTarget -Force
Add-Content -LiteralPath (Join-Path $stage 'OptiScaler.ini') -Value @'

[AmdRtgi]
Enabled=false
Quality=2
Denoiser=1
Inspect=0
Mix=1
Lighting=5
Occlusion=1
Ambient=1
Thickness=0.1
Smoothness=0.5
Fade=0.3
Fov=60
FarPlane=600
Contact=0
Saturation=1
Radius=1
'@
$hashes=Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object {$_.Name -ne 'SHA256SUMS.txt'} | Sort-Object FullName | ForEach-Object {
    '{0} *{1}' -f (Get-FileHash -LiteralPath $_.FullName).Hash,$_.FullName.Substring($stage.Length+1)
}
$hashes | Set-Content -LiteralPath (Join-Path $stage 'SHA256SUMS.txt')
$zip=Join-Path $PSScriptRoot ($PackageName+'.zip')
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal -Force
Write-Output $zip
