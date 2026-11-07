#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include <mutex>
#include "PtyBackend.h"
#include "TerminalBuffer.h"

enum class ConsoleMode
{
    Window,
    Quake,
};

class TerminalWindow
{
public:
    TerminalWindow();
    ~TerminalWindow();

    bool Create(HINSTANCE hInstance, ConsoleMode mode = ConsoleMode::Window);
    void Show();
    void Hide();
    void ToggleVisibility();
    void Close();

    void SetMode(ConsoleMode mode);
    ConsoleMode GetMode() const { return _mode; }
    bool IsQuakeWindow() const { return _mode == ConsoleMode::Quake; }

    void SummonWindow(bool toggleVisibility, int dropdownDurationMs);

    HWND GetHandle() const { return _hwnd; }
    bool IsVisible() const;

    static const wchar_t* const WindowClassName;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void _registerWindowClass(HINSTANCE hInstance);
    void _createWindow(HINSTANCE hInstance);
    void _initTerminal();
    void _setupFonts();
    void _paintTerminal(HDC hdc);
    void _updateWindowSize();

    void _enterQuakeMode();
    RECT _getQuakeModeRect();
    void _restoreWindowMode();

    void _onKeyDown(WPARAM vkKey, LPARAM lParam);
    void _sendCharToPty(wchar_t ch);
    void _onTerminalOutput(const char* data, size_t length);

    void _startDropdownAnimation(int durationMs);
    void _startSlideUpAnimation(int durationMs);

    LRESULT _messageHandler(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND _hwnd = nullptr;
    HFONT _hFont = nullptr;
    int _charWidth = 8;
    int _charHeight = 16;
    int _cols = 120;
    int _rows = 30;

    ConsoleMode _mode = ConsoleMode::Window;
    bool _isAnimating = false;

    std::unique_ptr<PtyBackend> _pty;
    std::unique_ptr<TerminalBuffer> _buffer;
    std::unique_ptr<VtParser> _parser;
    std::mutex _terminalMutex;

    RECT _savedWindowRect = {};
};
