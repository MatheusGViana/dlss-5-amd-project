#pragma once
#include "SysUtils.h"

class D3D11Hooks
{
  public:
    static void Hook(HMODULE dx11Module);
    static void HookToDevice(ID3D11Device* InDevice);
    static void Unhook();
};
