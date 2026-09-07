#pragma once
#include <windows.h>
#include <stdexcept>

inline static HWND g_hiddenSwapchainWindow = nullptr;
inline static constexpr wchar_t kHiddenSwapchainWindowClass[] = L"OptiDx11SwapchainHost";

inline static LRESULT CALLBACK HiddenSwapchainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline HWND CreateHiddenSwapchainWindow()
{
    if (g_hiddenSwapchainWindow != nullptr)
        return g_hiddenSwapchainWindow;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HiddenSwapchainWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kHiddenSwapchainWindowClass;

    if (!RegisterClassExW(&wc))
    {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("RegisterClassExW failed");
    }

    g_hiddenSwapchainWindow = CreateWindowExW(WS_EX_TOOLWINDOW, kHiddenSwapchainWindowClass, L"", WS_POPUP, 0, 0, 1, 1,
                                              nullptr, nullptr, wc.hInstance, nullptr);

    if (g_hiddenSwapchainWindow == nullptr)
        throw std::runtime_error("CreateWindowExW failed");

    ShowWindow(g_hiddenSwapchainWindow, SW_HIDE);
    return g_hiddenSwapchainWindow;
}

inline void DestroyHiddenSwapchainWindow()
{
    if (g_hiddenSwapchainWindow != nullptr)
    {
        DestroyWindow(g_hiddenSwapchainWindow);
        g_hiddenSwapchainWindow = nullptr;
    }
}
