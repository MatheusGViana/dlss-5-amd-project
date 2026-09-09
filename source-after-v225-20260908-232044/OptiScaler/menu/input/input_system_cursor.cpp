#include "pch.h"
#include "input_system_internal.h"

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#endif

#if defined(_MSC_VER)
#define OPTI_INPUT_RETURN_ADDRESS() _ReturnAddress()
#elif defined(__GNUC__) || defined(__clang__)
#define OPTI_INPUT_RETURN_ADDRESS() __builtin_return_address(0)
#else
#define OPTI_INPUT_RETURN_ADDRESS() nullptr
#endif

namespace OptiInput
{
RECT GetVirtualScreenRect()
{
    RECT rect {};

    rect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rect.right = rect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rect.bottom = rect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    return rect;
}

void BeginCursorClipBlockLocked()
{
    if (_state.CursorClipReleasedForMenu)
        return;

    LOG_INFO("begin cursor clip block input:{} target:{}", static_cast<void*>(_state.InputHwnd),
             static_cast<void*>(_state.TargetHwnd));

    RECT currentClip {};

    _state.HasSavedClipRect = false;
    _state.SavedClipWasActive = false;
    _state.SavedClipRect = {};

    if (o_GetClipCursor != nullptr && o_GetClipCursor(&currentClip))
    {
        _state.SavedClipRect = currentClip;
        _state.HasSavedClipRect = true;
        _state.SavedClipWasActive = !IsVirtualScreenRect(currentClip);
    }

    _state.DeferredClipRect = {};
    _state.HasDeferredClipRect = false;
    _state.DeferredClipIsNull = false;

    if (o_ClipCursor != nullptr)
        o_ClipCursor(nullptr);

    _state.CursorClipReleasedForMenu = true;
}

void EndCursorClipBlockLocked()
{
    if (!_state.CursorClipReleasedForMenu)
        return;

    LOG_INFO("end cursor clip block hasDeferred:{} deferredNull:{} hasSaved:{} savedActive:{}",
             _state.HasDeferredClipRect ? 1 : 0, _state.DeferredClipIsNull ? 1 : 0, _state.HasSavedClipRect ? 1 : 0,
             _state.SavedClipWasActive ? 1 : 0);

    if (_state.HasDeferredClipRect)
    {
        if (_state.DeferredClipIsNull)
        {
            if (o_ClipCursor != nullptr)
                o_ClipCursor(nullptr);
        }
        else
        {
            if (o_ClipCursor != nullptr)
                o_ClipCursor(&_state.DeferredClipRect);
        }
    }
    else if (_state.HasSavedClipRect && _state.SavedClipWasActive)
    {
        if (o_ClipCursor != nullptr)
            o_ClipCursor(&_state.SavedClipRect);
    }
    else
    {
        if (o_ClipCursor != nullptr)
            o_ClipCursor(nullptr);
    }

    _state.SavedClipRect = {};
    _state.HasSavedClipRect = false;
    _state.SavedClipWasActive = false;

    _state.DeferredClipRect = {};
    _state.HasDeferredClipRect = false;
    _state.DeferredClipIsNull = false;

    _state.CursorClipReleasedForMenu = false;
}

void StoreDeferredClipCursorLocked(const RECT* rect)
{
    _state.HasDeferredClipRect = true;

    if (rect == nullptr)
    {
        _state.DeferredClipRect = {};
        _state.DeferredClipIsNull = true;
        return;
    }

    _state.DeferredClipRect = *rect;
    _state.DeferredClipIsNull = false;
}

bool RectEquals(const RECT& a, const RECT& b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

bool IsVirtualScreenRect(const RECT& rect) { return RectEquals(rect, GetVirtualScreenRect()); }

namespace
{

bool TryGetModuleFromAddress(const void* address, HMODULE& module)
{
    module = nullptr;

    if (address == nullptr)
        return false;

    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(address), &module) != FALSE &&
           module != nullptr;
}

bool IsInternalCursorReadCaller(const void* callerAddress)
{
    HMODULE selfModule = nullptr;
    HMODULE callerModule = nullptr;

    if (!TryGetModuleFromAddress(reinterpret_cast<const void*>(&hkGetCursorPos), selfModule))
        return false;

    if (!TryGetModuleFromAddress(callerAddress, callerModule))
        return false;

    return callerModule == selfModule;
}

bool GetBlockedCursorScreenPosLocked(POINT& point)
{
    if (_state.ExternalVirtualMouseActive && _state.ExternalVirtualMouseInitialized && _state.ExternalTargetProcess &&
        _state.InputHwnd == nullptr && _state.TargetHwnd != nullptr && IsWindow(_state.TargetHwnd))
    {
        POINT screenPos = _state.ExternalVirtualMouseClient;
        if (ClientToScreen(_state.TargetHwnd, &screenPos))
        {
            _state.MouseScreenPos = screenPos;
            _state.ExternalGetCursorPosVirtualizedThisFrame = true;
            _state.ExternalGetCursorPosVirtualizedCount++;
            point = screenPos;
            return true;
        }
    }

    _state.ExternalGetCursorPosVirtualizedThisFrame = true;
    _state.ExternalGetCursorPosVirtualizedCount++;

    if (_state.HasBlockedCursorScreenPos)
        point = _state.BlockedCursorScreenPos;
    else
        point = _state.MouseScreenPos;

    return true;
}

DWORD PackCursorMessagePos(const POINT& point)
{
    return static_cast<DWORD>(MAKELONG(static_cast<SHORT>(point.x), static_cast<SHORT>(point.y)));
}
} // namespace

BOOL WINAPI hkGetCursorPos(LPPOINT point)
{
    if (point == nullptr)
        return FALSE;

    {
        std::unique_lock lock(_state.Mutex);

        if (ShouldBlockCursorInputLocked() && !IsInternalCursorReadCaller(OPTI_INPUT_RETURN_ADDRESS()))
        {
            _state.GetCursorPosBlockedCount++;
            GetBlockedCursorScreenPosLocked(*point);
            return TRUE;
        }
    }

    return o_GetCursorPos(point);
}

BOOL WINAPI hkSetCursorPos(int x, int y)
{
    {
        std::unique_lock lock(_state.Mutex);

        if (ShouldBlockCursorInputLocked())
        {
            _state.SetCursorPosBlockedCount++;
            return TRUE;
        }
    }

    return o_SetCursorPos(x, y);
}

BOOL WINAPI hkGetPhysicalCursorPos(LPPOINT point)
{
    if (point == nullptr)
        return FALSE;

    {
        std::unique_lock lock(_state.Mutex);

        if (ShouldBlockCursorInputLocked() && !IsInternalCursorReadCaller(OPTI_INPUT_RETURN_ADDRESS()))
        {
            _state.GetPhysicalCursorPosBlockedCount++;
            GetBlockedCursorScreenPosLocked(*point);
            return TRUE;
        }
    }

    return o_GetPhysicalCursorPos != nullptr ? o_GetPhysicalCursorPos(point) : o_GetCursorPos(point);
}

BOOL WINAPI hkSetPhysicalCursorPos(int x, int y)
{
    {
        std::unique_lock lock(_state.Mutex);

        if (ShouldBlockCursorInputLocked())
        {
            _state.SetPhysicalCursorPosBlockedCount++;
            return TRUE;
        }
    }

    return o_SetPhysicalCursorPos != nullptr ? o_SetPhysicalCursorPos(x, y) : o_SetCursorPos(x, y);
}

DWORD WINAPI hkGetMessagePos()
{
    {
        std::unique_lock lock(_state.Mutex);

        if (ShouldBlockCursorInputLocked() && !IsInternalCursorReadCaller(OPTI_INPUT_RETURN_ADDRESS()))
        {
            POINT point {};
            _state.GetMessagePosBlockedCount++;
            GetBlockedCursorScreenPosLocked(point);
            return PackCursorMessagePos(point);
        }
    }

    return o_GetMessagePos();
}

int WINAPI hkGetMouseMovePointsEx(UINT pointSize, LPMOUSEMOVEPOINT point, LPMOUSEMOVEPOINT buffer, int bufferPoints,
                                  DWORD resolution)
{
    {
        std::unique_lock lock(_state.Mutex);

        if ((ShouldBlockMouseInputLocked() || ShouldBlockCursorInputLocked()) &&
            !IsInternalCursorReadCaller(OPTI_INPUT_RETURN_ADDRESS()))
        {
            _state.MouseMovePointsBlockedCount++;
            return 0;
        }
    }

    return o_GetMouseMovePointsEx(pointSize, point, buffer, bufferPoints, resolution);
}

BOOL WINAPI hkClipCursor(const RECT* rect)
{
    {
        std::unique_lock lock(_state.Mutex);

        if (ShouldBlockCursorInputLocked())
        {
            _state.ClipCursorBlockedCount++;
            OPTIINPUT_LOG_VERBOSE("deferring ClipCursor rect:{}", rect != nullptr ? 1 : 0);
            StoreDeferredClipCursorLocked(rect);
            return TRUE;
        }
    }

    return o_ClipCursor(rect);
}

BOOL WINAPI hkGetClipCursor(LPRECT rect)
{
    if (rect == nullptr)
        return FALSE;

    {
        std::unique_lock lock(_state.Mutex);

        if (ShouldBlockCursorInputLocked())
        {
            _state.GetClipCursorVirtualizedCount++;
            OPTIINPUT_LOG_VERBOSE("virtualizing GetClipCursor");
            *rect = GetVirtualScreenRect();
            return TRUE;
        }
    }

    return o_GetClipCursor(rect);
}

} // namespace OptiInput
