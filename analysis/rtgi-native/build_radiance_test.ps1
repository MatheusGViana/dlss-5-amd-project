$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vc='F:/build/VC/Tools/MSVC/14.51.36231'
$sdk='C:/Program Files (x86)/Windows Kits/10'
Push-Location $root
try {
    python analysis/port_rtgi_radiance.py
    if($LASTEXITCODE -ne 0){throw 'Radiance translation failed'}
    foreach($entry in @('InitRadianceVolumeCS','PropagateRadianceCS0','PropagateRadianceCS1','PropagateRadianceCS2','PropagateRadianceCS3')) {
        & "$sdk/bin/10.0.26100.0/x64/fxc.exe" /nologo /T cs_5_0 /E $entry /Fo "analysis/rtgi-native/$entry.cso" analysis/rtgi-native/DiffuseRadiance.hlsl
        if($LASTEXITCODE -ne 0){throw "Shader compilation failed: $entry"}
    }
    & "$vc/bin/Hostx64/x64/cl.exe" /nologo /EHsc /std:c++20 /MD /O2 /Foanalysis/rtgi-native/radiance_test.obj "/I$vc/include" "/I$sdk/Include/10.0.26100.0/ucrt" "/I$sdk/Include/10.0.26100.0/shared" "/I$sdk/Include/10.0.26100.0/um" "/I$sdk/Include/10.0.26100.0/winrt" analysis/rtgi-native/radiance_test.cpp /Fe:analysis/rtgi-native/radiance_test.exe /link "/LIBPATH:$vc/lib/x64" "/LIBPATH:$sdk/Lib/10.0.26100.0/ucrt/x64" "/LIBPATH:$sdk/Lib/10.0.26100.0/um/x64" d3d12.lib dxgi.lib d3dcompiler.lib
    if($LASTEXITCODE -ne 0){throw 'GPU test compilation failed'}
    & analysis/rtgi-native/radiance_test.exe | Tee-Object analysis/rtgi-native/radiance-result.txt
    if($LASTEXITCODE -ne 0){throw 'Radiance GPU validation failed'}
} finally {Pop-Location}
