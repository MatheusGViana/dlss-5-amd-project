#pragma once

#include <Windows.h>

#include <cstdint>

namespace OptiInput
{
enum class InputAcquisitionMode : std::uint32_t
{
    None = 0,
    WindowMessages = 1,
    RawInput = 2,
    PolledAbsolute = 3,
    ExternalRawVirtualMouse = 4,
};

struct InitializeOptions
{
    // Target/game window. Can be foreign-process in external overlay mode.
    HWND TargetHwnd = nullptr;

    // Local input window. Null means TargetHwnd is used when it belongs to this process.
    HWND InputHwnd = nullptr;

    bool IsUwp = false;
    bool UseWndProcSubclass = true;
};

struct DebugState
{
    HWND TargetHwnd = nullptr;
    HWND TargetRootHwnd = nullptr;
    HWND InputHwnd = nullptr;
    HWND InputRootHwnd = nullptr;
    HWND RawMouseTargetHwnd = nullptr;
    HWND RawKeyboardTargetHwnd = nullptr;

    DWORD TargetProcessId = 0;
    DWORD TargetThreadId = 0;
    DWORD InputProcessId = 0;
    DWORD InputThreadId = 0;
    DWORD CurrentProcessId = 0;
    DWORD RawMouseFlags = 0;
    DWORD RawKeyboardFlags = 0;

    bool Initialized = false;
    bool HooksInstalled = false;
    bool Focused = false;

    bool MenuVisible = false;
    bool BlockMouse = false;
    bool BlockKeyboard = false;
    bool BlockCursor = false;

    bool IsUwp = false;
    bool UseWndProcSubclass = true;
    bool WndProcSubclassed = false;
    bool ExternalTargetProcess = false;
    bool HasExplicitInputHwnd = false;

    bool PolledInputActive = false;
    bool PolledInputUsedThisFrame = false;
    bool PolledMouseUsedThisFrame = false;
    bool PolledKeyboardUsedThisFrame = false;

    InputAcquisitionMode AcquisitionMode = InputAcquisitionMode::None;

    bool ExternalVirtualMouseActive = false;
    bool ExternalVirtualMouseUsedThisFrame = false;
    bool ExternalVirtualMouseRelativeUsedThisFrame = false;
    bool ExternalVirtualMouseAuthoritative = false;
    bool ExternalGetCursorPosVirtualizedThisFrame = false;
    bool ExternalCursorRecenteringDetected = false;
    bool ExternalLowLevelMouseHookInstalled = false;
    bool ExternalRawInputSinkRegistered = false;
    bool ExternalRawInputSinkPumpUsedThisFrame = false;

    bool ReceivedWindowMessageThisFrame = false;
    bool ReceivedQueueMessageThisFrame = false;
    bool ReceivedRawInputThisFrame = false;
    bool ReceivedAnyInputThisFrame = false;

    bool RawMouseRegistered = false;
    bool RawKeyboardRegistered = false;
    bool RawMouseNoLegacy = false;
    bool RawKeyboardNoLegacy = false;
    bool RawMouseInputSink = false;
    bool RawKeyboardInputSink = false;
    bool RawMouseCaptureMouse = false;

    std::uint32_t WindowsHookTrackedCount = 0;

    bool GameInputModuleLoaded = false;
    bool GameInputCreateExportFound = false;
    bool GameInputCreateHookInstalled = false;
    bool GameInputCreateHookAttempted = false;
    bool GameInputInterfaceSeen = false;
    bool WindowsGamingInputModuleLoaded = false;

    bool XInputModuleLoaded = false;
    bool XInputGetStateHookInstalled = false;
    bool XInputGetStateExHookInstalled = false;
    bool XInputGetKeystrokeHookInstalled = false;
    bool XInputSetStateHookInstalled = false;

    bool DirectInputModuleLoaded = false;
    bool DirectInputLegacyModuleLoaded = false;
    bool DirectInput8CreateHookInstalled = false;
    bool DirectInputCreateAHookInstalled = false;
    bool DirectInputCreateWHookInstalled = false;
    bool DirectInputCreateExHookInstalled = false;
    bool DirectInputCreateDeviceAHookInstalled = false;
    bool DirectInputCreateDeviceWHookInstalled = false;
    bool DirectInputGetDeviceStateHookInstalled = false;
    bool DirectInputGetDeviceDataHookInstalled = false;
    bool DirectInputDeviceReleaseHookInstalled = false;
    bool DirectInputKeyboardDeviceSeen = false;
    bool DirectInputMouseDeviceSeen = false;
    bool DirectInputOtherDeviceSeen = false;

    HRESULT GameInputLastCreateResult = S_OK;

    std::uint64_t GameInputCreateCallCount = 0;
    std::uint64_t GameInputCreateSucceededCount = 0;
    std::uint64_t GameInputCreateFailedCount = 0;

    std::uint64_t XInputGetStateCallCount = 0;
    std::uint64_t XInputGetStateBlockedCount = 0;
    std::uint64_t XInputGetStatePassedCount = 0;
    std::uint64_t XInputGetKeystrokeCallCount = 0;
    std::uint64_t XInputGetKeystrokeBlockedCount = 0;
    std::uint64_t XInputGetKeystrokePassedCount = 0;
    std::uint64_t XInputSetStateCallCount = 0;
    std::uint64_t XInputSetStateBlockedCount = 0;
    std::uint64_t XInputSetStatePassedCount = 0;

    std::uint64_t DirectInputCreateCallCount = 0;
    std::uint64_t DirectInputCreateSucceededCount = 0;
    std::uint64_t DirectInputCreateFailedCount = 0;
    std::uint64_t DirectInputCreateDeviceCallCount = 0;
    std::uint64_t DirectInputCreateDeviceSucceededCount = 0;
    std::uint64_t DirectInputCreateDeviceFailedCount = 0;
    std::uint64_t DirectInputTrackedDeviceCount = 0;
    std::uint64_t DirectInputGetDeviceStateCallCount = 0;
    std::uint64_t DirectInputGetDeviceStateBlockedCount = 0;
    std::uint64_t DirectInputGetDeviceStatePassedCount = 0;
    std::uint64_t DirectInputGetDeviceDataCallCount = 0;
    std::uint64_t DirectInputGetDeviceDataBlockedCount = 0;
    std::uint64_t DirectInputGetDeviceDataPassedCount = 0;

    bool HidMouseHandleSeen = false;
    bool HidKeyboardHandleSeen = false;
    bool HidGamepadHandleSeen = false;
    bool HidOtherHandleSeen = false;
    std::uint64_t HidCreateFileCallCount = 0;
    std::uint64_t HidTrackedHandleCount = 0;
    std::uint64_t HidReadFileCallCount = 0;
    std::uint64_t HidReadFileBlockedCount = 0;
    std::uint64_t HidReadFilePassedCount = 0;
    std::uint64_t HidDeviceIoControlCallCount = 0;
    std::uint64_t HidDeviceIoControlBlockedCount = 0;
    std::uint64_t HidDeviceIoControlPassedCount = 0;
    std::uint64_t MouseMovePointsBlockedCount = 0;

    std::uint64_t RawKeyboardSanitizedCount = 0;
    std::uint64_t RawKeyboardPassedCount = 0;
    std::uint64_t RawMouseSanitizedCount = 0;
    std::uint64_t RawMousePartialPassedCount = 0;
    std::uint64_t RawMousePassedCount = 0;

    std::uint64_t WindowsHookKeyboardBlockedCount = 0;
    std::uint64_t WindowsHookKeyboardPassedCount = 0;
    std::uint64_t WindowsHookMouseBlockedCount = 0;
    std::uint64_t WindowsHookMousePassedCount = 0;

    std::uint64_t PolledInputFrameCount = 0;
    std::uint64_t PolledMouseFrameCount = 0;
    std::uint64_t PolledKeyboardFrameCount = 0;

    std::uint64_t ExternalVirtualMouseFrameCount = 0;
    std::uint64_t ExternalVirtualMouseAuthoritativeFrameCount = 0;
    std::uint64_t ExternalGetCursorPosVirtualizedCount = 0;
    std::uint64_t ExternalVirtualMouseAbsoluteSuppressedCount = 0;
    std::uint64_t ExternalMouseDeltaEventCount = 0;
    std::uint64_t ExternalCursorRecenteringEventCount = 0;
    std::uint64_t ExternalRawInputSinkMessageCount = 0;
    std::uint64_t ExternalRawInputSinkPumpFrameCount = 0;

    POINT ExternalVirtualMouseClient = {};
    LONG ExternalPendingMouseDeltaX = 0;
    LONG ExternalPendingMouseDeltaY = 0;
};

bool Initialize(const InitializeOptions& options);
bool Initialize(HWND targetHwnd, bool isUwp = false);
bool Initialize(HWND targetHwnd, HWND inputHwnd, bool isUwp = false);

void Shutdown();

void BeginFrame(HWND targetHwnd, bool isUwp = false);
void BeginFrame(HWND targetHwnd, HWND inputHwnd, bool isUwp = false);
void FeedImGui(bool menuVisible);
void EndFrame(bool menuVisible);

void SetMenuVisible(bool visible);
void ResetMenuInputTransientState();

bool IsFocused();

DebugState GetDebugState();

bool IsKeyDown(int vk);
bool IsKeyPressed(int vk);
bool IsKeyReleased(int vk);
int GetLastPressedKey();

bool IsMouseDown(int button);
bool IsMousePressed(int button);
bool IsMouseReleased(int button);

float GetMouseWheel();
POINT GetMouseScreenPos();

bool ShouldBlockMouse();
bool ShouldBlockKeyboard();
bool ShouldBlockCursor();
bool ShouldBlockVirtualKey(int vk);

bool IsExternalVirtualMouseAuthoritative();
InputAcquisitionMode GetInputAcquisitionMode();

} // namespace OptiInput
