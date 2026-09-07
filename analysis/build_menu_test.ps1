$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$vc='F:/build/VC/Tools/MSVC/14.51.36231'
$sdk='C:/Program Files (x86)/Windows Kits/10'
$imgui=Join-Path $root 'OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler/include/imgui'
$freetype=Join-Path $root 'OptiScaler-DLSSNR-PreSR-Multipass-main/external/freetype'
$env:INCLUDE="$sdk/Include/10.0.26100.0/winrt;$freetype;"+$env:INCLUDE
$objects=Join-Path $PSScriptRoot ('menu-test-objects-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $objects | Out-Null
Push-Location $PSScriptRoot
try {
    & "$vc/bin/Hostx64/x64/cl.exe" /nologo /EHsc /std:c++20 /MD /O2 "/Fo$objects/" "/I$imgui" "/I$vc/include" "/I$sdk/Include/10.0.26100.0/ucrt" "/I$sdk/Include/10.0.26100.0/shared" "/I$sdk/Include/10.0.26100.0/um" menu_device_loss_test.cpp "$imgui/imgui.cpp" "$imgui/imgui_draw.cpp" "$imgui/imgui_tables.cpp" "$imgui/imgui_widgets.cpp" "$imgui/imgui_impl_dx12.cpp" "$imgui/misc/freetype/imgui_freetype.cpp" "$freetype/freetype.lib" /Fe:menu-device-loss-test.exe /link "/LIBPATH:$vc/lib/x64" "/LIBPATH:$sdk/Lib/10.0.26100.0/ucrt/x64" "/LIBPATH:$sdk/Lib/10.0.26100.0/um/x64" d3d12.lib dxgi.lib d3dcompiler.lib user32.lib imm32.lib
    if($LASTEXITCODE -ne 0){throw 'Menu test build failed'}
} finally {Pop-Location}
