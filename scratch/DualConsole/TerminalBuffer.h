#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <functional>

struct TerminalCell
{
    wchar_t ch = L' ';
    uint8_t fgColor = 7;
    uint8_t bgColor = 0;
    bool dirty = true;
};

class VtParser;

class TerminalBuffer
{
public:
    TerminalBuffer(int cols, int rows);

    void Resize(int cols, int rows);
    void Clear();

    void PutChar(wchar_t ch);
    void Newline();
    void CarriageReturn();
    void Backspace();
    void Tab();

    void SetCursorPos(int col, int row);
    void MoveCursorUp(int n);
    void MoveCursorDown(int n);
    void MoveCursorForward(int n);
    void MoveCursorBack(int n);

    void EraseInDisplay(int mode);
    void EraseInLine(int mode);

    void SetForegroundColor(uint8_t color);
    void SetBackgroundColor(uint8_t color);
    void ResetColors();

    int GetCols() const { return _cols; }
    int GetRows() const { return _rows; }
    int GetCursorX() const { return _cursorX; }
    int GetCursorY() const { return _cursorY; }
    uint8_t GetFgColor() const { return _currentFg; }
    uint8_t GetBgColor() const { return _currentBg; }

    const TerminalCell& GetCell(int col, int row) const;

    void SetScrollCallback(std::function<void()> callback) { _scrollCallback = callback; }

private:
    void _scrollUp();
    void _ensureSize(int col, int row);
    TerminalCell& _cell(int col, int row);

    std::vector<TerminalCell> _cells;
    int _cols;
    int _rows;
    int _cursorX = 0;
    int _cursorY = 0;
    uint8_t _currentFg = 7;
    uint8_t _currentBg = 0;
    std::function<void()> _scrollCallback;
};

class VtParser
{
public:
    explicit VtParser(TerminalBuffer& buffer);

    void Parse(const char* data, size_t length);

private:
    enum class State
    {
        Ground,
        EscapeStart,
        EscapeIntermediate,
        CsiEntry,
        CsiParam,
        OscString,
    };

    void _handleGround(char ch);
    void _handleEscape(char ch);
    void _handleCsi(char ch);
    void _handleOsc(char ch);
    void _executeCsi(char finalChar);
    int _getParam(int index, int defaultValue = 1);

    State _state = State::Ground;
    TerminalBuffer& _buffer;
    std::string _paramString;
    std::string _oscString;
    char _intermediateChar = 0;
};
