@echo off

if "%~1"=="" (
    echo Usage: %~nx0 ShaderName
    exit /b 1
)

set ShaderName=%1

echo Creating Dx12 CSO
"%~dp0dxc.exe" -T ps_5_1 -E PSMain -Cc -Qstrip_debug -Qstrip_reflect -Vi "%ShaderName%.hlsl" -Fo "%ShaderName%_PShader.cso"
"%~dp0dxc.exe" -T vs_5_1 -E VSMain -Cc -Qstrip_debug -Qstrip_reflect -Vi "%ShaderName%.hlsl" -Fo "%ShaderName%_VShader.cso"

echo Creating Dx12 Header
python "%~dp0create_header.py" "%ShaderName%_PShader.cso" "%ShaderName%_PShader.h" %ShaderName%_PS_cso
python "%~dp0create_header.py" "%ShaderName%_VShader.cso" "%ShaderName%_VShader.h" %ShaderName%_VS_cso
