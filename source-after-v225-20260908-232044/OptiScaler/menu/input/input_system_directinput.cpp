#include "pch.h"
#include "input_system_internal.h"

#include <detours/detours.h>

#include <cstring>

namespace OptiInput
{
namespace
{
constexpr wchar_t DirectInput8ModuleName[] = L"dinput8.dll";
constexpr wchar_t DirectInputLegacyModuleName[] = L"dinput.dll";

constexpr char DirectInput8CreateExportName[] = "DirectInput8Create";
constexpr char DirectInputCreateAExportName[] = "DirectInputCreateA";
constexpr char DirectInputCreateWExportName[] = "DirectInputCreateW";
constexpr char DirectInputCreateExExportName[] = "DirectInputCreateEx";

constexpr GUID DirectInputSysKeyboardGuid = {
    0x6f1d2b61, 0xd5a0, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 }
};
constexpr GUID DirectInputSysMouseGuid = {
    0x6f1d2b60, 0xd5a0, 0x11cf, { 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 }
};

bool IsDirectInputKeyboardGuid(REFGUID guid) { return IsEqualGUID(guid, DirectInputSysKeyboardGuid) != FALSE; }

bool IsDirectInputMouseGuid(REFGUID guid) { return IsEqualGUID(guid, DirectInputSysMouseGuid) != FALSE; }

DirectInputDeviceKind GetDirectInputDeviceKind(REFGUID guid)
{
    if (IsDirectInputKeyboardGuid(guid))
        return DirectInputDeviceKind::Keyboard;

    if (IsDirectInputMouseGuid(guid))
        return DirectInputDeviceKind::Mouse;

    return DirectInputDeviceKind::Other;
}

bool ShouldBlockDirectInputKeyboardLocked()
{
    return _state.Initialized && _state.Focused && ShouldBlockKeyboardInputLocked();
}

bool ShouldBlockDirectInputMouseLocked()
{
    return _state.Initialized && _state.Focused && ShouldBlockMouseInputLocked();
}

bool ShouldBlockDirectInputOtherLocked()
{
    return _state.Initialized && _state.Focused && ShouldApplyBlockingPolicyLocked() &&
           (_state.BlockKeyboard || _state.BlockMouse);
}

bool ShouldBlockDirectInputDeviceLocked(DirectInputDeviceKind kind)
{
    switch (kind)
    {
    case DirectInputDeviceKind::Keyboard:
        return ShouldBlockDirectInputKeyboardLocked();

    case DirectInputDeviceKind::Mouse:
        return ShouldBlockDirectInputMouseLocked();

    case DirectInputDeviceKind::Other:
    default:
        return ShouldBlockDirectInputOtherLocked();
    }
}

const char* DirectInputDeviceKindName(DirectInputDeviceKind kind)
{
    switch (kind)
    {
    case DirectInputDeviceKind::Keyboard:
        return "keyboard";

    case DirectInputDeviceKind::Mouse:
        return "mouse";

    case DirectInputDeviceKind::Other:
    default:
        return "other";
    }
}

HMODULE FindLoadedDirectInput8Module() { return GetModuleHandleW(DirectInput8ModuleName); }

HMODULE FindLoadedDirectInputLegacyModule() { return GetModuleHandleW(DirectInputLegacyModuleName); }

void ClearDirectInputHookPointersLocked()
{
    o_DirectInput8Create = nullptr;
    o_DirectInputCreateA = nullptr;
    o_DirectInputCreateW = nullptr;
    o_DirectInputCreateEx = nullptr;
    o_DirectInputCreateDeviceA = nullptr;
    o_DirectInputCreateDeviceW = nullptr;
    o_DirectInputDeviceGetDeviceState = nullptr;
    o_DirectInputDeviceGetDeviceData = nullptr;
    o_DirectInputDeviceRelease = nullptr;

    _state.DirectInput8CreateHookInstalled = false;
    _state.DirectInputCreateAHookInstalled = false;
    _state.DirectInputCreateWHookInstalled = false;
    _state.DirectInputCreateExHookInstalled = false;
    _state.DirectInputCreateDeviceAHookInstalled = false;
    _state.DirectInputCreateDeviceWHookInstalled = false;
    _state.DirectInputGetDeviceStateHookInstalled = false;
    _state.DirectInputGetDeviceDataHookInstalled = false;
    _state.DirectInputDeviceReleaseHookInstalled = false;
}

std::size_t FindDirectInputDeviceSlotLocked(void* device)
{
    if (device == nullptr)
        return MaxTrackedDirectInputDevices;

    for (std::size_t i = 0; i < _state.DirectInputDeviceSlots.size(); ++i)
    {
        if (_state.DirectInputDeviceSlots[i].InUse && _state.DirectInputDeviceSlots[i].Device == device)
            return i;
    }

    return MaxTrackedDirectInputDevices;
}

DirectInputDeviceKind GetDirectInputDeviceKindLocked(void* device)
{
    const std::size_t slot = FindDirectInputDeviceSlotLocked(device);

    if (slot >= MaxTrackedDirectInputDevices)
        return DirectInputDeviceKind::Other;

    return _state.DirectInputDeviceSlots[slot].Kind;
}

void ClearDirectInputDeviceSlotLocked(std::size_t slot)
{
    if (slot >= MaxTrackedDirectInputDevices)
        return;

    if (_state.DirectInputDeviceSlots[slot].InUse && _state.DirectInputTrackedDeviceCount > 0)
        _state.DirectInputTrackedDeviceCount--;

    _state.DirectInputDeviceSlots[slot] = {};
}

void ClearAllDirectInputDeviceSlotsLocked()
{
    _state.DirectInputDeviceSlots = {};
    _state.DirectInputTrackedDeviceCount = 0;
}

void TrackDirectInputDeviceLocked(void* device, DirectInputDeviceKind kind)
{
    if (device == nullptr)
        return;

    std::size_t freeSlot = MaxTrackedDirectInputDevices;

    for (std::size_t i = 0; i < _state.DirectInputDeviceSlots.size(); ++i)
    {
        auto& slot = _state.DirectInputDeviceSlots[i];

        if (slot.InUse && slot.Device == device)
        {
            slot.Kind = kind;
            return;
        }

        if (!slot.InUse && freeSlot >= MaxTrackedDirectInputDevices)
            freeSlot = i;
    }

    if (freeSlot >= MaxTrackedDirectInputDevices)
    {
        LOG_WARN("DirectInput device tracking table is full device:{} kind:{}", device,
                 DirectInputDeviceKindName(kind));
        return;
    }

    auto& slot = _state.DirectInputDeviceSlots[freeSlot];
    slot.InUse = true;
    slot.Device = device;
    slot.Kind = kind;

    _state.DirectInputTrackedDeviceCount++;

    if (kind == DirectInputDeviceKind::Keyboard)
        _state.DirectInputKeyboardDeviceSeen = true;
    else if (kind == DirectInputDeviceKind::Mouse)
        _state.DirectInputMouseDeviceSeen = true;
    else
        _state.DirectInputOtherDeviceSeen = true;

    LOG_INFO("DirectInput device captured device:{} kind:{}", device, DirectInputDeviceKindName(kind));
}

bool HookDirectInputDeviceLocked(void* device, DirectInputDeviceKind kind)
{
    if (device == nullptr)
        return false;

    PVOID* vtable = *reinterpret_cast<PVOID**>(device);

    auto release = reinterpret_cast<DirectInputDeviceRelease_t>(vtable[2]);
    auto getDeviceState = reinterpret_cast<DirectInputGetDeviceState_t>(vtable[9]);
    auto getDeviceData = reinterpret_cast<DirectInputGetDeviceData_t>(vtable[10]);

    bool attachRelease = false;
    bool attachGetDeviceState = false;
    bool attachGetDeviceData = false;

    if (o_DirectInputDeviceRelease == nullptr)
    {
        o_DirectInputDeviceRelease = release;
        attachRelease = o_DirectInputDeviceRelease != nullptr;
    }
    else if (o_DirectInputDeviceRelease != release)
    {
        LOG_WARN("DirectInput device Release pointer differs, not detouring new pointer device:{}", device);
    }

    if (o_DirectInputDeviceGetDeviceState == nullptr)
    {
        o_DirectInputDeviceGetDeviceState = getDeviceState;
        attachGetDeviceState = o_DirectInputDeviceGetDeviceState != nullptr;
    }
    else if (o_DirectInputDeviceGetDeviceState != getDeviceState)
    {
        LOG_WARN("DirectInput GetDeviceState pointer differs, not detouring new pointer device:{}", device);
    }

    if (o_DirectInputDeviceGetDeviceData == nullptr)
    {
        o_DirectInputDeviceGetDeviceData = getDeviceData;
        attachGetDeviceData = o_DirectInputDeviceGetDeviceData != nullptr;
    }
    else if (o_DirectInputDeviceGetDeviceData != getDeviceData)
    {
        LOG_WARN("DirectInput GetDeviceData pointer differs, not detouring new pointer device:{}", device);
    }

    if (!attachRelease && !attachGetDeviceState && !attachGetDeviceData)
    {
        TrackDirectInputDeviceLocked(device, kind);
        return true;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (attachRelease)
        DetourAttach(reinterpret_cast<PVOID*>(&o_DirectInputDeviceRelease), hkDirectInputDeviceRelease);

    if (attachGetDeviceState)
        DetourAttach(reinterpret_cast<PVOID*>(&o_DirectInputDeviceGetDeviceState), hkDirectInputGetDeviceState);

    if (attachGetDeviceData)
        DetourAttach(reinterpret_cast<PVOID*>(&o_DirectInputDeviceGetDeviceData), hkDirectInputGetDeviceData);

    const LONG result = DetourTransactionCommit();

    if (result != NO_ERROR)
    {
        LOG_ERROR("DirectInput device hook installation failed result:{} device:{} kind:{}", result, device,
                  DirectInputDeviceKindName(kind));

        if (attachRelease)
            o_DirectInputDeviceRelease = nullptr;

        if (attachGetDeviceState)
            o_DirectInputDeviceGetDeviceState = nullptr;

        if (attachGetDeviceData)
            o_DirectInputDeviceGetDeviceData = nullptr;

        return false;
    }

    if (attachRelease)
        _state.DirectInputDeviceReleaseHookInstalled = true;

    if (attachGetDeviceState)
        _state.DirectInputGetDeviceStateHookInstalled = true;

    if (attachGetDeviceData)
        _state.DirectInputGetDeviceDataHookInstalled = true;

    TrackDirectInputDeviceLocked(device, kind);
    return true;
}

bool HookDirectInputInterfaceLocked(void* directInput, bool wide)
{
    if (directInput == nullptr)
        return false;

    PVOID* vtable = *reinterpret_cast<PVOID**>(directInput);
    auto createDevice = reinterpret_cast<DirectInputCreateDevice_t>(vtable[3]);

    if (createDevice == nullptr)
        return false;

    if (wide)
    {
        if (_state.DirectInputCreateDeviceWHookInstalled)
        {
            if (o_DirectInputCreateDeviceW != createDevice)
                LOG_WARN("DirectInput W CreateDevice pointer changed, existing hook remains active old:{} new:{}",
                         reinterpret_cast<void*>(o_DirectInputCreateDeviceW), reinterpret_cast<void*>(createDevice));

            return true;
        }

        o_DirectInputCreateDeviceW = createDevice;
    }
    else
    {
        if (_state.DirectInputCreateDeviceAHookInstalled)
        {
            if (o_DirectInputCreateDeviceA != createDevice)
                LOG_WARN("DirectInput A CreateDevice pointer changed, existing hook remains active old:{} new:{}",
                         reinterpret_cast<void*>(o_DirectInputCreateDeviceA), reinterpret_cast<void*>(createDevice));

            return true;
        }

        o_DirectInputCreateDeviceA = createDevice;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (wide)
        DetourAttach(reinterpret_cast<PVOID*>(&o_DirectInputCreateDeviceW), hkDirectInputCreateDeviceW);
    else
        DetourAttach(reinterpret_cast<PVOID*>(&o_DirectInputCreateDeviceA), hkDirectInputCreateDeviceA);

    const LONG result = DetourTransactionCommit();

    if (result != NO_ERROR)
    {
        LOG_ERROR("DirectInput CreateDevice hook installation failed result:{} wide:{}", result, wide ? 1 : 0);

        if (wide)
            o_DirectInputCreateDeviceW = nullptr;
        else
            o_DirectInputCreateDeviceA = nullptr;

        return false;
    }

    if (wide)
        _state.DirectInputCreateDeviceWHookInstalled = true;
    else
        _state.DirectInputCreateDeviceAHookInstalled = true;

    LOG_INFO("DirectInput CreateDevice hook installed wide:{}", wide ? 1 : 0);
    return true;
}

bool TryGetDirectInputInterfaceWidth(REFIID riid, bool* wide)
{
    if (wide == nullptr)
        return false;

    if (IsEqualGUID(riid, IID_IDirectInput8W) || IsEqualGUID(riid, IID_IDirectInput7W) ||
        IsEqualGUID(riid, IID_IDirectInput2W) || IsEqualGUID(riid, IID_IDirectInputW))
    {
        *wide = true;
        return true;
    }

    if (IsEqualGUID(riid, IID_IDirectInput8A) || IsEqualGUID(riid, IID_IDirectInput7A) ||
        IsEqualGUID(riid, IID_IDirectInput2A) || IsEqualGUID(riid, IID_IDirectInputA))
    {
        *wide = false;
        return true;
    }

    return false;
}

void HandleDirectInputCreatedLocked(REFIID riid, void** out, const char* source)
{
    if (out == nullptr || *out == nullptr)
        return;

    bool wide = false;
    if (TryGetDirectInputInterfaceWidth(riid, &wide))
    {
        HookDirectInputInterfaceLocked(*out, wide);
        return;
    }

    OPTIINPUT_LOG_VERBOSE("{} returned unsupported riid directInput:{} riid:{}",
                          source != nullptr ? source : "DirectInput", *out, static_cast<const void*>(&riid));
}

void HandleLegacyDirectInputCreatedLocked(void** out, bool wide)
{
    if (out == nullptr || *out == nullptr)
        return;

    HookDirectInputInterfaceLocked(*out, wide);
}

bool InstallDirectInputExportHookLocked(HMODULE module, const char* exportName, void** original, void* hook,
                                        bool* installed)
{
    if (module == nullptr || exportName == nullptr || original == nullptr || hook == nullptr || installed == nullptr)
        return false;

    if (*installed)
        return true;

    *original = reinterpret_cast<void*>(GetProcAddress(module, exportName));

    if (*original == nullptr)
        return false;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<PVOID*>(original), hook);

    const LONG result = DetourTransactionCommit();

    if (result != NO_ERROR)
    {
        LOG_ERROR("{} hook installation failed result:{}", exportName, result);
        *original = nullptr;
        return false;
    }

    *installed = true;
    LOG_INFO("{} hook installed module:{}", exportName, static_cast<void*>(module));
    return true;
}

HRESULT CallDirectInputCreateDeviceOriginal(DirectInputCreateDevice_t original, void* directInput, REFGUID guid,
                                            void** device, LPUNKNOWN outer)
{
    if (original == nullptr)
        return DIERR_GENERIC;

    ScopedHookBypass bypass;
    return original(directInput, guid, device, outer);
}
} // namespace

void UpdateDirectInputIntegrationLocked()
{
    HMODULE module8 = FindLoadedDirectInput8Module();
    HMODULE legacyModule = FindLoadedDirectInputLegacyModule();

    _state.DirectInputModule = module8;
    _state.DirectInputLegacyModule = legacyModule;
    _state.DirectInputModuleLoaded = module8 != nullptr || legacyModule != nullptr;
    _state.DirectInputLegacyModuleLoaded = legacyModule != nullptr;

    if (module8 != nullptr)
    {
        if (!InstallDirectInputExportHookLocked(module8, DirectInput8CreateExportName,
                                                reinterpret_cast<void**>(&o_DirectInput8Create), hkDirectInput8Create,
                                                &_state.DirectInput8CreateHookInstalled))
        {
            if (o_DirectInput8Create == nullptr)
            {
                OPTIINPUT_LOG_VERBOSE("DirectInput8Create export was not found module:{}", static_cast<void*>(module8));
            }
        }
    }

    if (legacyModule != nullptr)
    {
        InstallDirectInputExportHookLocked(legacyModule, DirectInputCreateAExportName,
                                           reinterpret_cast<void**>(&o_DirectInputCreateA), hkDirectInputCreateA,
                                           &_state.DirectInputCreateAHookInstalled);

        InstallDirectInputExportHookLocked(legacyModule, DirectInputCreateWExportName,
                                           reinterpret_cast<void**>(&o_DirectInputCreateW), hkDirectInputCreateW,
                                           &_state.DirectInputCreateWHookInstalled);

        InstallDirectInputExportHookLocked(legacyModule, DirectInputCreateExExportName,
                                           reinterpret_cast<void**>(&o_DirectInputCreateEx), hkDirectInputCreateEx,
                                           &_state.DirectInputCreateExHookInstalled);
    }
}

void RemoveDirectInputHooksLocked()
{
    if (!_state.DirectInput8CreateHookInstalled && !_state.DirectInputCreateAHookInstalled &&
        !_state.DirectInputCreateWHookInstalled && !_state.DirectInputCreateExHookInstalled &&
        !_state.DirectInputCreateDeviceAHookInstalled && !_state.DirectInputCreateDeviceWHookInstalled &&
        !_state.DirectInputGetDeviceStateHookInstalled && !_state.DirectInputGetDeviceDataHookInstalled &&
        !_state.DirectInputDeviceReleaseHookInstalled)
    {
        ClearDirectInputHookPointersLocked();
        ClearAllDirectInputDeviceSlotsLocked();
        return;
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (_state.DirectInput8CreateHookInstalled && o_DirectInput8Create != nullptr)
        DetourDetach(reinterpret_cast<PVOID*>(&o_DirectInput8Create), hkDirectInput8Create);

    if (_state.DirectInputCreateAHookInstalled && o_DirectInputCreateA != nullptr)
        DetourDetach(reinterpret_cast<PVOID*>(&o_DirectInputCreateA), hkDirectInputCreateA);

    if (_state.DirectInputCreateWHookInstalled && o_DirectInputCreateW != nullptr)
        DetourDetach(reinterpret_cast<PVOID*>(&o_DirectInputCreateW), hkDirectInputCreateW);

    if (_state.DirectInputCreateExHookInstalled && o_DirectInputCreateEx != nullptr)
        DetourDetach(reinterpret_cast<PVOID*>(&o_DirectInputCreateEx), hkDirectInputCreateEx);

    if (_state.DirectInputCreateDeviceAHookInstalled && o_DirectInputCreateDeviceA != nullptr)
        DetourDetach(reinterpret_cast<PVOID*>(&o_DirectInputCreateDeviceA), hkDirectInputCreateDeviceA);

    if (_state.DirectInputCreateDeviceWHookInstalled && o_DirectInputCreateDeviceW != nullptr)
        DetourDetach(reinterpret_cast<PVOID*>(&o_DirectInputCreateDeviceW), hkDirectInputCreateDeviceW);

    if (_state.DirectInputGetDeviceStateHookInstalled && o_DirectInputDeviceGetDeviceState != nullptr)
        DetourDetach(reinterpret_cast<PVOID*>(&o_DirectInputDeviceGetDeviceState), hkDirectInputGetDeviceState);

    if (_state.DirectInputGetDeviceDataHookInstalled && o_DirectInputDeviceGetDeviceData != nullptr)
        DetourDetach(reinterpret_cast<PVOID*>(&o_DirectInputDeviceGetDeviceData), hkDirectInputGetDeviceData);

    if (_state.DirectInputDeviceReleaseHookInstalled && o_DirectInputDeviceRelease != nullptr)
        DetourDetach(reinterpret_cast<PVOID*>(&o_DirectInputDeviceRelease), hkDirectInputDeviceRelease);

    const LONG result = DetourTransactionCommit();

    if (result != NO_ERROR)
        LOG_WARN("DirectInput hook removal completed with result:{}", result);

    ClearDirectInputHookPointersLocked();
    ClearAllDirectInputDeviceSlotsLocked();
}

HRESULT WINAPI hkDirectInput8Create(HINSTANCE instance, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer)
{
    HRESULT result = DIERR_GENERIC;

    if (o_DirectInput8Create != nullptr)
    {
        ScopedHookBypass bypass;
        result = o_DirectInput8Create(instance, version, riid, out, outer);
    }

    {
        std::unique_lock lock(_state.Mutex);
        _state.DirectInputCreateCallCount++;

        if (SUCCEEDED(result))
        {
            _state.DirectInputCreateSucceededCount++;
            HandleDirectInputCreatedLocked(riid, reinterpret_cast<void**>(out), "DirectInput8Create");
        }
        else
        {
            _state.DirectInputCreateFailedCount++;
        }
    }

    return result;
}

HRESULT WINAPI hkDirectInputCreateA(HINSTANCE instance, DWORD version, void** out, LPUNKNOWN outer)
{
    HRESULT result = DIERR_GENERIC;

    if (o_DirectInputCreateA != nullptr)
    {
        ScopedHookBypass bypass;
        result = o_DirectInputCreateA(instance, version, out, outer);
    }

    {
        std::unique_lock lock(_state.Mutex);
        _state.DirectInputCreateCallCount++;

        if (SUCCEEDED(result))
        {
            _state.DirectInputCreateSucceededCount++;
            HandleLegacyDirectInputCreatedLocked(out, false);
        }
        else
        {
            _state.DirectInputCreateFailedCount++;
        }
    }

    return result;
}

HRESULT WINAPI hkDirectInputCreateW(HINSTANCE instance, DWORD version, void** out, LPUNKNOWN outer)
{
    HRESULT result = DIERR_GENERIC;

    if (o_DirectInputCreateW != nullptr)
    {
        ScopedHookBypass bypass;
        result = o_DirectInputCreateW(instance, version, out, outer);
    }

    {
        std::unique_lock lock(_state.Mutex);
        _state.DirectInputCreateCallCount++;

        if (SUCCEEDED(result))
        {
            _state.DirectInputCreateSucceededCount++;
            HandleLegacyDirectInputCreatedLocked(out, true);
        }
        else
        {
            _state.DirectInputCreateFailedCount++;
        }
    }

    return result;
}

HRESULT WINAPI hkDirectInputCreateEx(HINSTANCE instance, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer)
{
    HRESULT result = DIERR_GENERIC;

    if (o_DirectInputCreateEx != nullptr)
    {
        ScopedHookBypass bypass;
        result = o_DirectInputCreateEx(instance, version, riid, out, outer);
    }

    {
        std::unique_lock lock(_state.Mutex);
        _state.DirectInputCreateCallCount++;

        if (SUCCEEDED(result))
        {
            _state.DirectInputCreateSucceededCount++;
            HandleDirectInputCreatedLocked(riid, reinterpret_cast<void**>(out), "DirectInputCreateEx");
        }
        else
        {
            _state.DirectInputCreateFailedCount++;
        }
    }

    return result;
}

HRESULT WINAPI hkDirectInputCreateDeviceA(void* directInput, REFGUID guid, void** device, LPUNKNOWN outer)
{
    HRESULT result = CallDirectInputCreateDeviceOriginal(o_DirectInputCreateDeviceA, directInput, guid, device, outer);

    {
        std::unique_lock lock(_state.Mutex);
        _state.DirectInputCreateDeviceCallCount++;

        if (SUCCEEDED(result) && device != nullptr && *device != nullptr)
        {
            _state.DirectInputCreateDeviceSucceededCount++;
            HookDirectInputDeviceLocked(*device, GetDirectInputDeviceKind(guid));
        }
        else if (FAILED(result))
        {
            _state.DirectInputCreateDeviceFailedCount++;
        }
    }

    return result;
}

HRESULT WINAPI hkDirectInputCreateDeviceW(void* directInput, REFGUID guid, void** device, LPUNKNOWN outer)
{
    HRESULT result = CallDirectInputCreateDeviceOriginal(o_DirectInputCreateDeviceW, directInput, guid, device, outer);

    {
        std::unique_lock lock(_state.Mutex);
        _state.DirectInputCreateDeviceCallCount++;

        if (SUCCEEDED(result) && device != nullptr && *device != nullptr)
        {
            _state.DirectInputCreateDeviceSucceededCount++;
            HookDirectInputDeviceLocked(*device, GetDirectInputDeviceKind(guid));
        }
        else if (FAILED(result))
        {
            _state.DirectInputCreateDeviceFailedCount++;
        }
    }

    return result;
}

HRESULT WINAPI hkDirectInputGetDeviceState(void* device, DWORD dataSize, LPVOID data)
{
    {
        std::unique_lock lock(_state.Mutex);
        const DirectInputDeviceKind kind = GetDirectInputDeviceKindLocked(device);
        _state.DirectInputGetDeviceStateCallCount++;

        if (ShouldBlockDirectInputDeviceLocked(kind))
        {
            if (data != nullptr && dataSize > 0)
                std::memset(data, 0, dataSize);

            _state.DirectInputGetDeviceStateBlockedCount++;
            OPTIINPUT_LOG_VERBOSE("blocking DirectInput GetDeviceState device:{} kind:{} size:{}", device,
                                  DirectInputDeviceKindName(kind), dataSize);
            return DI_OK;
        }

        _state.DirectInputGetDeviceStatePassedCount++;
    }

    if (o_DirectInputDeviceGetDeviceState == nullptr)
        return DIERR_GENERIC;

    ScopedHookBypass bypass;
    return o_DirectInputDeviceGetDeviceState(device, dataSize, data);
}

HRESULT WINAPI hkDirectInputGetDeviceData(void* device, DWORD objectDataSize, LPDIDEVICEOBJECTDATA data, LPDWORD inOut,
                                          DWORD flags)
{
    {
        std::unique_lock lock(_state.Mutex);
        const DirectInputDeviceKind kind = GetDirectInputDeviceKindLocked(device);
        _state.DirectInputGetDeviceDataCallCount++;

        if (ShouldBlockDirectInputDeviceLocked(kind))
        {
            if (inOut != nullptr)
                *inOut = 0;

            _state.DirectInputGetDeviceDataBlockedCount++;
            OPTIINPUT_LOG_VERBOSE("blocking DirectInput GetDeviceData device:{} kind:{} flags:{}", device,
                                  DirectInputDeviceKindName(kind), flags);
            return DI_OK;
        }

        _state.DirectInputGetDeviceDataPassedCount++;
    }

    if (o_DirectInputDeviceGetDeviceData == nullptr)
        return DIERR_GENERIC;

    ScopedHookBypass bypass;
    return o_DirectInputDeviceGetDeviceData(device, objectDataSize, data, inOut, flags);
}

ULONG WINAPI hkDirectInputDeviceRelease(void* device)
{
    ULONG result = 0;

    if (o_DirectInputDeviceRelease != nullptr)
    {
        ScopedHookBypass bypass;
        result = o_DirectInputDeviceRelease(device);
    }

    if (result == 0)
    {
        std::unique_lock lock(_state.Mutex);
        const std::size_t slot = FindDirectInputDeviceSlotLocked(device);

        if (slot < MaxTrackedDirectInputDevices)
            ClearDirectInputDeviceSlotLocked(slot);
    }

    return result;
}

} // namespace OptiInput
