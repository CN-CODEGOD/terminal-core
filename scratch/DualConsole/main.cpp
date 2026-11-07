#include <windows.h>
#include <stdio.h>
#include <string>
#include "TerminalWindow.h"
#include "SummonManager.h"

static const wchar_t* const HiddenMessageClassName = L"RightShellSummonHost";

struct AppState
{
    HINSTANCE hInstance = nullptr;
    TerminalWindow windowConsole;
    TerminalWindow quakeConsole;
    SummonManager summon;
    HWND messageWindow = nullptr;
    bool running = true;
};

static AppState g_state;

static LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_HOTKEY:
    {
        g_state.summon.HandleHotkey(static_cast<int>(wParam));
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

static HWND CreateMessageWindow(HINSTANCE hInstance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = HiddenWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = HiddenMessageClassName;
    RegisterClassExW(&wc);

    return CreateWindowExW(0, HiddenMessageClassName, L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr);
}

static void PrintUsage()
{
    wprintf(L"=== RightShell Dual Console ===\n\n");
    wprintf(L"Hotkeys:\n");
    wprintf(L"  Ctrl+`        Toggle Window Mode console\n");
    wprintf(L"  Ctrl+Shift+`  Toggle Quake Mode console\n");
    wprintf(L"  Alt+`         Switch between Window and Quake mode\n");
    wprintf(L"\nWindow Mode: Standard terminal window\n");
    wprintf(L"Quake Mode:  Drop-down terminal from top of screen\n");
    wprintf(L"\nPress Ctrl+C to exit.\n\n");
}

static void SetupSummonHotkeys()
{
    g_state.summon.AddSummonHotkey(
        MOD_CONTROL, VK_OEM_3,
        []() {
            g_state.windowConsole.SummonWindow(true, 200);
        });

    g_state.summon.AddSummonHotkey(
        MOD_CONTROL | MOD_SHIFT, VK_OEM_3,
        []() {
            g_state.quakeConsole.SummonWindow(true, 200);
        });

    g_state.summon.AddSummonHotkey(
        MOD_ALT, VK_OEM_3,
        []() {
            if (g_state.windowConsole.IsVisible())
            {
                g_state.windowConsole.Hide();
                g_state.quakeConsole.SetMode(ConsoleMode::Quake);
                g_state.quakeConsole.SummonWindow(false, 200);
            }
            else if (g_state.quakeConsole.IsVisible())
            {
                g_state.quakeConsole.Hide();
                g_state.windowConsole.SetMode(ConsoleMode::Window);
                g_state.windowConsole.SummonWindow(false, 0);
            }
            else
            {
                g_state.windowConsole.SummonWindow(false, 0);
            }
        });
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    g_state.hInstance = hInstance;

    AllocConsole();
    FILE* f;
    _wfreopen_s(&f, L"CONOUT$", L"w", stdout);
    PrintUsage();

    if (!g_state.windowConsole.Create(hInstance, ConsoleMode::Window))
    {
        wprintf(L"Failed to create Window Mode console.\n");
        return 1;
    }

    if (!g_state.quakeConsole.Create(hInstance, ConsoleMode::Quake))
    {
        wprintf(L"Failed to create Quake Mode console.\n");
        return 1;
    }

    g_state.messageWindow = CreateMessageWindow(hInstance);
    if (!g_state.messageWindow)
    {
        wprintf(L"Failed to create message window.\n");
        return 1;
    }

    g_state.summon.Initialize(g_state.messageWindow);
    SetupSummonHotkeys();

    g_state.windowConsole.Show();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_state.summon.Shutdown();

    if (g_state.messageWindow)
    {
        DestroyWindow(g_state.messageWindow);
    }

    FreeConsole();

    return 0;
}
