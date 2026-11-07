#pragma once

#include <windows.h>
#include <vector>
#include <functional>

struct HotkeyBinding
{
    UINT modifiers;
    UINT vk;
    int id;
    std::function<void()> callback;
};

class SummonManager
{
public:
    SummonManager();
    ~SummonManager();

    bool Initialize(HWND messageWindow);
    void Shutdown();

    void AddSummonHotkey(UINT modifiers, UINT vk, std::function<void()> callback);
    void RemoveAllHotkeys();

    void HandleHotkey(int hotkeyId);

    static const int HOTKEY_ID_WINDOW_MODE = 100;
    static const int HOTKEY_ID_QUAKE_MODE = 101;
    static const int HOTKEY_ID_TOGGLE_WINDOW = 102;
    static const int HOTKEY_ID_TOGGLE_QUAKE = 103;
    static const int HOTKEY_ID_SWITCH_MODE = 104;

private:
    HWND _messageWindow = nullptr;
    std::vector<HotkeyBinding> _hotkeys;
    int _nextHotkeyId = 200;
};
