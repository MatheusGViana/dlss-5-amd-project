param([string]$GameDir=$PSScriptRoot)
$ErrorActionPreference='Stop'
$game=(Resolve-Path -LiteralPath $GameDir).Path
Write-Output 'OptiScaler AMD Pre-SR v1.6 - diagnostico somente leitura'
Write-Output ('Data: '+(Get-Date -Format o))
Write-Output ('Pasta: '+$game)
Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,Status | Format-List
foreach($name in @('dxgi.dll','winmm.dll','OptiScaler.dll','version.dll','dlssnr_amd_pass1.dll','dlssnr_amd_pass2.dll','dlssnr_amd_pass3.dll','dlssnr_on_amd_weights.bin')) {
    $file=Join-Path $game $name
    if(Test-Path -LiteralPath $file -PathType Leaf) {
        $item=Get-Item -LiteralPath $file
        Write-Output ("Arquivo {0} bytes={1} SHA256={2}" -f $name,$item.Length,(Get-FileHash -LiteralPath $file).Hash)
    } else {Write-Output ('Ausente: '+$name)}
}
foreach($dir in @($game,[Environment]::SystemDirectory)) {
    foreach($name in @('amdhip64_7.dll','amdhip64_6.dll','vcruntime140.dll','vcruntime140_1.dll','msvcp140.dll')) {
        $file=Join-Path $dir $name
        if(Test-Path -LiteralPath $file) {
            $item=Get-Item -LiteralPath $file
            Write-Output ("Runtime {0} versao={1}" -f $file,$item.VersionInfo.FileVersion)
        }
    }
}
foreach($name in @('amd_presr.log','dlssnr_on_amd.log','OptiScaler.log')) {
    $file=Join-Path $game $name
    Write-Output ('Log: '+$name)
    if(Test-Path -LiteralPath $file) {Get-Content -LiteralPath $file -Tail 60} else {Write-Output 'Log ausente'}
}
