$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'OptiScaler-DLSSNR-PreSR-Multipass-main'
$sdk='C:\PROGRA~2\WINDOW~1\10\'
Set-Content -LiteralPath (Join-Path $source 'OptiScaler/resource_build_date.h') -Value ('#define VER_BUILD_DATE "'+(Get-Date -Format 'yyyyMMdd_HHmmss')+'"')
Set-Content -LiteralPath (Join-Path $source 'OptiScaler/resource_build_commit.h') -Value '#define VER_BUILD_COMMIT "amd-presr-multipass-local"'
Push-Location $source
try {
& 'F:/build/MSBuild/Current/Bin/MSBuild.exe' OptiScaler.sln /m:4 /t:Build /p:Configuration=Release /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0.26100.0 "/p:WindowsSdkDir=$sdk" "/p:UniversalCRTSdkDir=$sdk" /p:UCRTVersion=10.0.26100.0 "/p:TargetPlatformSdkPath=$sdk" /p:PreBuildEventUseInBuild=false /p:PostBuildEventUseInBuild=false /v:minimal /fl "/flp:logfile=$PSScriptRoot/analysis/build-amd.log;verbosity=normal"
if($LASTEXITCODE -ne 0){throw 'OptiScaler build failed; see analysis/build-amd.log'}
} finally {Pop-Location}
