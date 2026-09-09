#pragma once

#include "SysUtils.h"

#include <Util.h>
#include <Config.h>

#include "Ntdll_Proxy.h"
#include "KernelBase_Proxy.h"

#include <detours/detours.h>

// List of functions to proxy
#define PROXY_FUNCTIONS(X)                                                                                             \
    X(FreeLibrary)                                                                                                     \
    X(LoadLibraryA)                                                                                                    \
    X(LoadLibraryW)                                                                                                    \
    X(LoadLibraryExA)                                                                                                  \
    X(LoadLibraryExW)                                                                                                  \
    X(GetProcAddress)                                                                                                  \
    X(GetModuleHandleA)                                                                                                \
    X(GetModuleHandleW)                                                                                                \
    X(GetModuleHandleExA)                                                                                              \
    X(GetModuleHandleExW)                                                                                              \
    X(GetFileAttributesW)                                                                                              \
    X(CreateFileW)                                                                                                     \
    X(OutputDebugStringA)                                                                                              \
    X(OutputDebugStringW)

class Kernel32Proxy
{
  public:
    // typedef decltype(&FreeLibrary) PFN_FreeLibrary;
#define DEFINE_TYPEDEF(name) typedef decltype(&name) PFN_##name;
    PROXY_FUNCTIONS(DEFINE_TYPEDEF)
#undef DEFINE_TYPEDEF

    static void Init()
    {
        if (_dll != nullptr)
            return;

        _dll = KernelBaseProxy::GetModuleHandleW_()(L"kernel32.dll");
        if (_dll == nullptr)
            _dll = NtdllProxy::LoadLibraryExW_Ldr(L"kernel32.dll", NULL, 0);

        if (_dll == nullptr)
            return;

        // _FreeLibrary = (PFN_FreeLibrary) KernelBaseProxy::GetProcAddress_()(_dll, "FreeLibrary");
#define INIT_FUNC(name) _##name = (PFN_##name) KernelBaseProxy::GetProcAddress_()(_dll, #name);
        PROXY_FUNCTIONS(INIT_FUNC)
#undef INIT_FUNC
    }

    static HMODULE Module() { return _dll; }

    // static PFN_FreeLibrary FreeLibrary_() { return _FreeLibrary; }
    // static PFN_FreeLibrary FreeLibrary_Hooked(){return (PFN_FreeLibrary) KernelBaseProxy::GetProcAddress_()(_dll,
    // "FreeLibrary");}
#define DEFINE_GETTERS(name)                                                                                           \
    static PFN_##name name##_() { return _##name; }                                                                    \
    static PFN_##name name##_Hooked() { return (PFN_##name) KernelBaseProxy::GetProcAddress_()(_dll, #name); }

    PROXY_FUNCTIONS(DEFINE_GETTERS)
#undef DEFINE_GETTERS

    // Hook_FreeLibrary
#define DEFINE_HOOK(name)                                                                                              \
    static PFN_##name Hook_##name(PVOID method)                                                                        \
    {                                                                                                                  \
        auto addr = name##_Hooked();                                                                                   \
        DetourTransactionBegin();                                                                                      \
        DetourUpdateThread(GetCurrentThread());                                                                        \
        DetourAttach(&(PVOID&) addr, method);                                                                          \
        auto detourResult = DetourTransactionCommit();                                                                 \
        if (detourResult != NO_ERROR)                                                                                  \
        {                                                                                                              \
            LOG_ERROR("Failed to hook: {:X}", detourResult);                                                           \
            return nullptr;                                                                                            \
        }                                                                                                              \
        _##name = addr;                                                                                                \
        return addr;                                                                                                   \
    }

    PROXY_FUNCTIONS(DEFINE_HOOK)
#undef DEFINE_HOOK

  private:
    inline static HMODULE _dll = nullptr;

    // inline static PFN_FreeLibrary _FreeLibrary = nullptr;
#define DEFINE_STORAGE(name) inline static PFN_##name _##name = nullptr;
    PROXY_FUNCTIONS(DEFINE_STORAGE)
#undef DEFINE_STORAGE
};

#undef PROXY_FUNCTIONS
