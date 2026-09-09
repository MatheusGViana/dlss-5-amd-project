$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vc='F:/build/VC/Tools/MSVC/14.51.36231'
$sdk='C:/Program Files (x86)/Windows Kits/10'
Push-Location $root
try {
    & "$vc/bin/Hostx64/x64/cl.exe" /nologo /EHsc /std:c++20 /MD /O2 /Foanalysis/rtgi-native/temporal_test.obj "/I$vc/include" "/I$sdk/Include/10.0.26100.0/ucrt" "/I$sdk/Include/10.0.26100.0/shared" "/I$sdk/Include/10.0.26100.0/um" "/I$sdk/Include/10.0.26100.0/winrt" analysis/rtgi-native/temporal_test.cpp /Fe:analysis/rtgi-native/temporal_test.exe /link "/LIBPATH:$vc/lib/x64" "/LIBPATH:$sdk/Lib/10.0.26100.0/ucrt/x64" "/LIBPATH:$sdk/Lib/10.0.26100.0/um/x64" d3d12.lib dxgi.lib d3dcompiler.lib
    if($LASTEXITCODE -ne 0){throw 'GPU test compilation failed'}
    & analysis/rtgi-native/temporal_test.exe | Tee-Object analysis/rtgi-native/temporal-result.txt
    if($LASTEXITCODE -ne 0){throw 'Trace GPU validation failed'}
} finally {Pop-Location}
