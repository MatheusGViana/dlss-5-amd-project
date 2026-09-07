$ErrorActionPreference='Stop'
$root = Split-Path -Parent $PSScriptRoot
$vc='F:/build/VC/Tools/MSVC/14.51.36231'
$sdk='C:/Program Files (x86)/Windows Kits/10'
$inc=Join-Path $root 'OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler/dlssnr/amd'
Push-Location $PSScriptRoot
try {
& "$vc/bin/Hostx64/x64/cl.exe" /nologo /EHsc /std:c++20 /MD /O2 "/I$inc" "/I$vc/include" "/I$sdk/Include/10.0.26100.0/ucrt" "/I$sdk/Include/10.0.26100.0/shared" "/I$sdk/Include/10.0.26100.0/um" "/I$sdk/Include/10.0.26100.0/winrt" smoke.cpp "$inc/AmdPreSr.cpp" /Fe:amd-smoke.exe /link "/LIBPATH:$vc/lib/x64" "/LIBPATH:$sdk/Lib/10.0.26100.0/ucrt/x64" "/LIBPATH:$sdk/Lib/10.0.26100.0/um/x64" d3d12.lib dxgi.lib d3dcompiler.lib bcrypt.lib user32.lib
if($LASTEXITCODE -ne 0){throw 'Smoke compilation failed'}
} finally {Pop-Location}

