#include "TerminalWindow.h"
#include <dwmapi.h>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")

const wchar_t* const TerminalWindow::WindowClassName = L"RightShellDualConsole";

static COLORREF VgaColorRef(uint8_t index)
{
    static const COLORREF colors[8] = {
        RGB(12, 12, 12),
        RGB(197, 15, 31),
        RGB(19, 161, 14),
        RGB(193, 156, 0),
        RGB(0, 55, 218),
        RGB(136, 23, 152),
        RGB(58, 150, 221),
        RGB(204, 204, 204),
    };
    return colors[index % 8];
}

static COLORREF BrightVgaColorRef(uint8_t index)
{
    static const COLORREF colors[8] = {
        RGB(118, 118, 118),
        RGB(231, 72, 86),
        RGB(22, 198, 12),
        RGB(249, 241, 165),
        RGB(59, 120, 255),
        RGB(180, 0, 158),
        RGB(97, 214, 214),
        RGB(242, 242, 242),
    };
    return colors[index % 8];
}

static COLORREF GetFgColor(uint8_t colorIndex)
{
    if (colorIndex < 8)
        return VgaColorRef(colorIndex);
    return BrightVgaColorRef(colorIndex - 8);
}

TerminalWindow::TerminalWindow() = default;

TerminalWindow::~TerminalWindow()
{
    if (_hFont)
    {
        DeleteObject(_hFont);
    }
    if (_hwnd)
    {
        DestroyWindow(_hwnd);
    }
}

bool TerminalWindow::Create(HINSTANCE hInstance, ConsoleMode mode)
{
    _mode = mode;
    _registerWindowClass(hInstance);
    _createWindow(hInstance);

    if (!_hwnd)
    {
        return false;
    }

    _setupFonts();
    _initTerminal();

    if (_mode == ConsoleMode::Quake)
    {
        _enterQuakeMode();
    }

    return true;
}

void TerminalWindow::_registerWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(12, 12, 12));
    wc.lpszClassName = WindowClassName;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExW(&wc);
}

void TerminalWindow::_createWindow(HINSTANCE hInstance)
{
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = WS_EX_APPWINDOW;

    int width = _cols * _charWidth + GetSystemMetrics(SM_CXFRAME) * 2;
    int height = _rows * _charHeight + GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME) * 2;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);

    int x = (screenWidth - width) / 2;
    int y = 100;

    _hwnd = CreateWindowExW(
        exStyle,
        WindowClassName,
        L"RightShell - Window Mode",
        style,
        x, y, width, height,
        nullptr, nullptr, hInstance, nullptr);

    SetWindowLongPtrW(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

void TerminalWindow::_setupFonts()
{
    if (_hFont)
    {
        DeleteObject(_hFont);
    }

    HDC hdc = GetDC(_hwnd);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(_hwnd, hdc);

    int fontSize = 14;
    _hFont = CreateFontW(
        -MulDiv(fontSize, dpiY, 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        L"Cascadia Code");

    if (!_hFont)
    {
        _hFont = CreateFontW(
            -MulDiv(fontSize, dpiY, 72),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
            L"Consolas");
    }

    HDC memDc = CreateCompatibleDC(nullptr);
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDc, _hFont));

    TEXTMETRICW tm;
    GetTextMetricsW(memDc, &tm);
    _charWidth = tm.tmAveCharWidth;
    _charHeight = tm.tmHeight;

    SelectObject(memDc, oldFont);
    DeleteDC(memDc);
}

void TerminalWindow::_initTerminal()
{
    _buffer = std::make_unique<TerminalBuffer>(_cols, _rows);
    _parser = std::make_unique<VtParser>(*_buffer);
    _pty = std::make_unique<PtyBackend>();

    _buffer->SetScrollCallback([this]() {
        if (_hwnd)
        {
            InvalidateRect(_hwnd, nullptr, FALSE);
        }
    });

    _pty->SetOutputCallback([this](const char* data, size_t length) {
        _onTerminalOutput(data, length);
    });

    _pty->Start(L"", _cols, _rows);
}

void TerminalWindow::_onTerminalOutput(const char* data, size_t length)
{
    {
        std::lock_guard<std::mutex> lock(_terminalMutex);
        _parser->Parse(data, length);
    }

    if (_hwnd)
    {
        InvalidateRect(_hwnd, nullptr, FALSE);
    }
}

void TerminalWindow::_paintTerminal(HDC hdc)
{
    std::lock_guard<std::mutex> lock(_terminalMutex);

    if (!_buffer) return;

    RECT clientRect;
    GetClientRect(_hwnd, &clientRect);

    HBRUSH bgBrush = CreateSolidBrush(RGB(12, 12, 12));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, _hFont));
    SetBkMode(hdc, TRANSPARENT);

    int cellW = _charWidth;
    int cellH = _charHeight;
    int cols = _buffer->GetCols();
    int rows = _buffer->GetRows();

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < cols; ++c)
        {
            const auto& cell = _buffer->GetCell(c, r);

            int x = c * cellW;
            int y = r * cellH;

            RECT cellRect = { x, y, x + cellW, y + cellH };

            COLORREF bgColor = VgaColorRef(cell.bgColor);
            if (cell.bgColor != 0)
            {
                HBRUSH cellBg = CreateSolidBrush(bgColor);
                FillRect(hdc, &cellRect, cellBg);
                DeleteObject(cellBg);
            }

            if (cell.ch != L' ' && cell.ch != L'\0')
            {
                COLORREF fgColor = GetFgColor(cell.fgColor);
                SetTextColor(hdc, fgColor);

                wchar_t text[2] = { cell.ch, L'\0' };
                TextOutW(hdc, x, y, text, 1);
            }
        }
    }

    int cursorX = _buffer->GetCursorX();
    int cursorY = _buffer->GetCursorY();
    RECT cursorRect = {
        cursorX * cellW,
        cursorY * cellH,
        cursorX * cellW + cellW,
        cursorY * cellH + cellH
    };

    InvertRect(hdc, &cursorRect);

    SelectObject(hdc, oldFont);
}

void TerminalWindow::_updateWindowSize()
{
    if (!_hwnd || !_buffer) return;

    RECT clientRect;
    GetClientRect(_hwnd, &clientRect);

    int newCols = std::max(1, static_cast<int>(clientRect.right) / _charWidth);
    int newRows = std::max(1, static_cast<int>(clientRect.bottom) / _charHeight);

    if (newCols != _cols || newRows != _rows)
    {
        _cols = newCols;
        _rows = newRows;
        _buffer->Resize(_cols, _rows);
        if (_pty && _pty->IsRunning())
        {
            _pty->Resize(_cols, _rows);
        }
    }
}

void TerminalWindow::_enterQuakeMode()
{
    _mode = ConsoleMode::Quake;

    if (_hwnd)
    {
        SetWindowTextW(_hwnd, L"RightShell - Quake Mode");
    }

    RECT quakeRect = _getQuakeModeRect();

    SetWindowLongW(_hwnd, GWL_STYLE,
        WS_POPUP | WS_VISIBLE | WS_THICKFRAME);
    SetWindowLongW(_hwnd, GWL_EXSTYLE,
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_APPWINDOW);

    SetWindowPos(_hwnd, HWND_TOPMOST,
        quakeRect.left, quakeRect.top,
        quakeRect.right - quakeRect.left,
        quakeRect.bottom - quakeRect.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    _updateWindowSize();
}

RECT TerminalWindow::_getQuakeModeRect()
{
    POINT pt = { 0, 0 };
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoW(hMon, &mi);

    int width = mi.rcWork.right - mi.rcWork.left;
    int height = (mi.rcWork.bottom - mi.rcWork.top) / 2;

    RECT rect;
    rect.left = mi.rcWork.left;
    rect.top = mi.rcWork.top;
    rect.right = mi.rcWork.left + width;
    rect.bottom = mi.rcWork.top + height;

    return rect;
}

void TerminalWindow::_restoreWindowMode()
{
    _mode = ConsoleMode::Window;

    if (_hwnd)
    {
        SetWindowTextW(_hwnd, L"RightShell - Window Mode");
    }

    SetWindowLongW(_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    SetWindowLongW(_hwnd, GWL_EXSTYLE, WS_EX_APPWINDOW);

    int width = _cols * _charWidth + GetSystemMetrics(SM_CXFRAME) * 2;
    int height = _rows * _charHeight + GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME) * 2;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);

    SetWindowPos(_hwnd, HWND_NOTOPMOST,
        (screenWidth - width) / 2, 100,
        width, height,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    _updateWindowSize();
}

void TerminalWindow::SetMode(ConsoleMode mode)
{
    if (mode == _mode) return;

    if (mode == ConsoleMode::Quake)
    {
        _enterQuakeMode();
    }
    else
    {
        _restoreWindowMode();
    }
}

void TerminalWindow::Show()
{
    if (_hwnd)
    {
        ShowWindow(_hwnd, SW_SHOW);
        SetForegroundWindow(_hwnd);
    }
}

void TerminalWindow::Hide()
{
    if (_hwnd)
    {
        ShowWindow(_hwnd, SW_HIDE);
    }
}

void TerminalWindow::ToggleVisibility()
{
    if (IsVisible())
    {
        Hide();
    }
    else
    {
        Show();
    }
}

bool TerminalWindow::IsVisible() const
{
    return _hwnd && IsWindowVisible(_hwnd);
}

void TerminalWindow::SummonWindow(bool toggleVisibility, int dropdownDurationMs)
{
    if (toggleVisibility && IsVisible())
    {
        HWND foreground = GetForegroundWindow();
        if (foreground == _hwnd)
        {
            if (dropdownDurationMs > 0)
            {
                _startSlideUpAnimation(dropdownDurationMs);
            }
            else
            {
                Hide();
            }
            return;
        }
    }

    if (dropdownDurationMs > 0)
    {
        Show();
        _startDropdownAnimation(dropdownDurationMs);
    }
    else
    {
        Show();
    }
}

void TerminalWindow::_startDropdownAnimation(int durationMs)
{
    if (_isAnimating) return;
    _isAnimating = true;

    RECT rect;
    GetWindowRect(_hwnd, &rect);
    int targetHeight = rect.bottom - rect.top;
    int targetWidth = rect.right - rect.left;
    int targetX = rect.left;
    int targetY = rect.top;

    int frameCount = std::max(1, durationMs / 16);
    int startHeight = 0;
    int startY = targetY;

    for (int i = 0; i <= frameCount; ++i)
    {
        float progress = static_cast<float>(i) / frameCount;
        float eased = progress * progress * (3.0f - 2.0f * progress);

        int currentHeight = static_cast<int>(startHeight + (targetHeight - startHeight) * eased);

        HRGN hRgn = CreateRectRgn(0, 0, targetWidth, currentHeight);
        SetWindowRgn(_hwnd, hRgn, TRUE);
        DeleteObject(hRgn);

        SetWindowPos(_hwnd, nullptr,
            targetX, startY,
            targetWidth, currentHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);

        UpdateWindow(_hwnd);
        Sleep(16);
    }

    SetWindowRgn(_hwnd, nullptr, TRUE);

    SetWindowPos(_hwnd, nullptr,
        targetX, targetY,
        targetWidth, targetHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    _isAnimating = false;
}

void TerminalWindow::_startSlideUpAnimation(int durationMs)
{
    if (_isAnimating) return;
    _isAnimating = true;

    RECT rect;
    GetWindowRect(_hwnd, &rect);
    int startHeight = rect.bottom - rect.top;
    int targetWidth = rect.right - rect.left;
    int targetX = rect.left;
    int targetY = rect.top;

    int frameCount = std::max(1, durationMs / 16);

    for (int i = 0; i <= frameCount; ++i)
    {
        float progress = static_cast<float>(i) / frameCount;
        float eased = progress * progress * (3.0f - 2.0f * progress);

        int currentHeight = static_cast<int>(startHeight * (1.0f - eased));

        HRGN hRgn = CreateRectRgn(0, 0, targetWidth, currentHeight);
        SetWindowRgn(_hwnd, hRgn, TRUE);
        DeleteObject(hRgn);

        SetWindowPos(_hwnd, nullptr,
            targetX, targetY,
            targetWidth, currentHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);

        UpdateWindow(_hwnd);
        Sleep(16);
    }

    Hide();
    SetWindowRgn(_hwnd, nullptr, TRUE);

    _isAnimating = false;
}

void TerminalWindow::_onKeyDown(WPARAM vkKey, LPARAM lParam)
{
    if (!_pty || !_pty->IsRunning()) return;

    BYTE keyState[256];
    GetKeyboardState(keyState);

    bool ctrl = (keyState[VK_CONTROL] & 0x80) != 0;

    switch (vkKey)
    {
    case VK_UP:
        _pty->WriteInput("\x1b[A", 3);
        return;
    case VK_DOWN:
        _pty->WriteInput("\x1b[B", 3);
        return;
    case VK_RIGHT:
        _pty->WriteInput("\x1b[C", 3);
        return;
    case VK_LEFT:
        _pty->WriteInput("\x1b[D", 3);
        return;
    case VK_HOME:
        _pty->WriteInput("\x1b[H", 3);
        return;
    case VK_END:
        _pty->WriteInput("\x1b[F", 3);
        return;
    case VK_DELETE:
        _pty->WriteInput("\x1b[3~", 4);
        return;
    case VK_INSERT:
        _pty->WriteInput("\x1b[2~", 4);
        return;
    case VK_PRIOR:
        _pty->WriteInput("\x1b[5~", 4);
        return;
    case VK_NEXT:
        _pty->WriteInput("\x1b[6~", 4);
        return;
    case VK_F1:
        _pty->WriteInput("\x1bOP", 3);
        return;
    case VK_F2:
        _pty->WriteInput("\x1bOQ", 3);
        return;
    case VK_F3:
        _pty->WriteInput("\x1bOR", 3);
        return;
    case VK_F4:
        _pty->WriteInput("\x1bOS", 3);
        return;
    case VK_F5:
        _pty->WriteInput("\x1b[15~", 4);
        return;
    case VK_F6:
        _pty->WriteInput("\x1b[17~", 4);
        return;
    case VK_F7:
        _pty->WriteInput("\x1b[18~", 4);
        return;
    case VK_F8:
        _pty->WriteInput("\x1b[19~", 4);
        return;
    case VK_F9:
        _pty->WriteInput("\x1b[20~", 4);
        return;
    case VK_F10:
        _pty->WriteInput("\x1b[21~", 4);
        return;
    case VK_F11:
        _pty->WriteInput("\x1b[23~", 4);
        return;
    case VK_F12:
        _pty->WriteInput("\x1b[24~", 4);
        return;
    default:
        break;
    }

    if (ctrl && vkKey >= 'A' && vkKey <= 'Z')
    {
        char ctrlChar = static_cast<char>(vkKey - 'A' + 1);
        _pty->WriteInput(&ctrlChar, 1);
    }
}

void TerminalWindow::_sendCharToPty(wchar_t ch)
{
    if (ch < 128)
    {
        char c = static_cast<char>(ch);
        _pty->WriteInput(&c, 1);
    }
    else
    {
        char utf8[4];
        int len = WideCharToMultiByte(CP_UTF8, 0, &ch, 1, utf8, 4, nullptr, nullptr);
        if (len > 0)
        {
            _pty->WriteInput(utf8, len);
        }
    }
}

void TerminalWindow::Close()
{
    if (_pty)
    {
        _pty->Stop();
    }
    if (_hwnd)
    {
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
}

LRESULT CALLBACK TerminalWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<TerminalWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (self)
    {
        return self->_messageHandler(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TerminalWindow::_messageHandler(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(_hwnd, &ps);

        HDC memDc = CreateCompatibleDC(hdc);
        RECT clientRect;
        GetClientRect(_hwnd, &clientRect);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDc, memBmp));

        _paintTerminal(memDc);

        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDc, 0, 0, SRCCOPY);

        SelectObject(memDc, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDc);

        EndPaint(_hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
    {
        if (wParam != SIZE_MINIMIZED)
        {
            _updateWindowSize();
            InvalidateRect(_hwnd, nullptr, FALSE);
        }
        else if (IsQuakeWindow())
        {
            ShowWindow(_hwnd, SW_HIDE);
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        _onKeyDown(wParam, lParam);
        return 0;
    }

    case WM_CHAR:
    {
        if (wParam >= 0x20)
        {
            _sendCharToPty(static_cast<wchar_t>(wParam));
        }
        return 0;
    }

    case WM_SETFOCUS:
    {
        InvalidateRect(_hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_KILLFOCUS:
    {
        InvalidateRect(_hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOVING:
    {
        if (IsQuakeWindow())
        {
            RECT* pRect = reinterpret_cast<RECT*>(lParam);
            RECT quakeRect = _getQuakeModeRect();
            *pRect = quakeRect;
        }
        return TRUE;
    }

    case WM_SIZING:
    {
        if (IsQuakeWindow())
        {
            int edge = static_cast<int>(wParam);
            if (edge != WMSZ_BOTTOM && edge != WMSZ_TOPLEFT && edge != WMSZ_TOPRIGHT)
            {
                RECT* pRect = reinterpret_cast<RECT*>(lParam);
                RECT current;
                GetWindowRect(_hwnd, &current);
                *pRect = current;
            }
        }
        return TRUE;
    }

    case WM_GETMINMAXINFO:
    {
        if (IsQuakeWindow())
        {
            RECT quakeRect = _getQuakeModeRect();
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMaxPosition.x = quakeRect.left;
            mmi->ptMaxPosition.y = quakeRect.top;
            mmi->ptMaxSize.x = quakeRect.right - quakeRect.left;
            mmi->ptMaxSize.y = quakeRect.bottom - quakeRect.top;
        }
        return 0;
    }

    case WM_WINDOWPOSCHANGING:
    {
        if (IsQuakeWindow())
        {
            WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
            RECT quakeRect = _getQuakeModeRect();
            wp->x = quakeRect.left;
            wp->y = quakeRect.top;
            wp->cx = quakeRect.right - quakeRect.left;
            wp->cy = quakeRect.bottom - quakeRect.top;
            wp->flags &= ~SWP_NOMOVE;
            wp->flags &= ~SWP_NOSIZE;
        }
        return 0;
    }

    case WM_CLOSE:
    {
        Close();
        return 0;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProcW(_hwnd, msg, wParam, lParam);
    }
}
