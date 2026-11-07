#include "TerminalBuffer.h"
#include <algorithm>
#include <cstring>

static const uint8_t ANSI_TO_VGA[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

static COLORREF VgaColor(uint8_t index, bool bright = false)
{
    static const COLORREF normalColors[8] = {
        RGB(12, 12, 12),
        RGB(197, 15, 31),
        RGB(19, 161, 14),
        RGB(193, 156, 0),
        RGB(0, 55, 218),
        RGB(136, 23, 152),
        RGB(58, 150, 221),
        RGB(204, 204, 204),
    };
    static const COLORREF brightColors[8] = {
        RGB(118, 118, 118),
        RGB(231, 72, 86),
        RGB(22, 198, 12),
        RGB(249, 241, 165),
        RGB(59, 120, 255),
        RGB(180, 0, 158),
        RGB(97, 214, 214),
        RGB(242, 242, 242),
    };
    return bright ? brightColors[index] : normalColors[index];
}

// --- TerminalBuffer ---

TerminalBuffer::TerminalBuffer(int cols, int rows) :
    _cols(cols),
    _rows(rows),
    _cells(cols * rows)
{
}

void TerminalBuffer::Resize(int cols, int rows)
{
    std::vector<TerminalCell> newCells(cols * rows);
    int copyCols = std::min(_cols, cols);
    int copyRows = std::min(_rows, rows);

    for (int r = 0; r < copyRows; ++r)
    {
        for (int c = 0; c < copyCols; ++c)
        {
            newCells[r * cols + c] = _cells[r * _cols + c];
        }
    }

    _cells = std::move(newCells);
    _cols = cols;
    _rows = rows;
    _cursorX = std::min(_cursorX, _cols - 1);
    _cursorY = std::min(_cursorY, _rows - 1);
}

void TerminalBuffer::Clear()
{
    for (auto& cell : _cells)
    {
        cell.ch = L' ';
        cell.fgColor = 7;
        cell.bgColor = 0;
        cell.dirty = true;
    }
    _cursorX = 0;
    _cursorY = 0;
}

void TerminalBuffer::PutChar(wchar_t ch)
{
    if (_cursorX >= _cols)
    {
        _cursorX = 0;
        _cursorY++;
        if (_cursorY >= _rows)
        {
            _scrollUp();
            _cursorY = _rows - 1;
        }
    }

    auto& cell = _cell(_cursorX, _cursorY);
    cell.ch = ch;
    cell.fgColor = _currentFg;
    cell.bgColor = _currentBg;
    cell.dirty = true;

    _cursorX++;
}

void TerminalBuffer::Newline()
{
    _cursorX = 0;
    _cursorY++;
    if (_cursorY >= _rows)
    {
        _scrollUp();
        _cursorY = _rows - 1;
    }
}

void TerminalBuffer::CarriageReturn()
{
    _cursorX = 0;
}

void TerminalBuffer::Backspace()
{
    if (_cursorX > 0)
    {
        _cursorX--;
    }
}

void TerminalBuffer::Tab()
{
    int nextTab = ((_cursorX / 8) + 1) * 8;
    if (nextTab >= _cols)
    {
        Newline();
    }
    else
    {
        while (_cursorX < nextTab)
        {
            PutChar(L' ');
        }
    }
}

void TerminalBuffer::SetCursorPos(int col, int row)
{
    _cursorX = std::max(0, std::min(col, _cols - 1));
    _cursorY = std::max(0, std::min(row, _rows - 1));
}

void TerminalBuffer::MoveCursorUp(int n)
{
    _cursorY = std::max(0, _cursorY - n);
}

void TerminalBuffer::MoveCursorDown(int n)
{
    _cursorY = std::min(_rows - 1, _cursorY + n);
}

void TerminalBuffer::MoveCursorForward(int n)
{
    _cursorX = std::min(_cols - 1, _cursorX + n);
}

void TerminalBuffer::MoveCursorBack(int n)
{
    _cursorX = std::max(0, _cursorX - n);
}

void TerminalBuffer::EraseInDisplay(int mode)
{
    switch (mode)
    {
    case 0:
        for (int c = _cursorX; c < _cols; ++c)
        {
            auto& cell = _cell(c, _cursorY);
            cell.ch = L' ';
            cell.bgColor = _currentBg;
            cell.dirty = true;
        }
        for (int r = _cursorY + 1; r < _rows; ++r)
        {
            for (int c = 0; c < _cols; ++c)
            {
                auto& cell = _cell(c, r);
                cell.ch = L' ';
                cell.bgColor = _currentBg;
                cell.dirty = true;
            }
        }
        break;
    case 1:
        for (int r = 0; r < _cursorY; ++r)
        {
            for (int c = 0; c < _cols; ++c)
            {
                auto& cell = _cell(c, r);
                cell.ch = L' ';
                cell.bgColor = _currentBg;
                cell.dirty = true;
            }
        }
        for (int c = 0; c <= _cursorX && c < _cols; ++c)
        {
            auto& cell = _cell(c, _cursorY);
            cell.ch = L' ';
            cell.bgColor = _currentBg;
            cell.dirty = true;
        }
        break;
    case 2:
    case 3:
        for (auto& cell : _cells)
        {
            cell.ch = L' ';
            cell.bgColor = _currentBg;
            cell.dirty = true;
        }
        break;
    }
}

void TerminalBuffer::EraseInLine(int mode)
{
    switch (mode)
    {
    case 0:
        for (int c = _cursorX; c < _cols; ++c)
        {
            auto& cell = _cell(c, _cursorY);
            cell.ch = L' ';
            cell.bgColor = _currentBg;
            cell.dirty = true;
        }
        break;
    case 1:
        for (int c = 0; c <= _cursorX && c < _cols; ++c)
        {
            auto& cell = _cell(c, _cursorY);
            cell.ch = L' ';
            cell.bgColor = _currentBg;
            cell.dirty = true;
        }
        break;
    case 2:
        for (int c = 0; c < _cols; ++c)
        {
            auto& cell = _cell(c, _cursorY);
            cell.ch = L' ';
            cell.bgColor = _currentBg;
            cell.dirty = true;
        }
        break;
    }
}

void TerminalBuffer::SetForegroundColor(uint8_t color)
{
    _currentFg = color % 8;
}

void TerminalBuffer::SetBackgroundColor(uint8_t color)
{
    _currentBg = color % 8;
}

void TerminalBuffer::ResetColors()
{
    _currentFg = 7;
    _currentBg = 0;
}

const TerminalCell& TerminalBuffer::GetCell(int col, int row) const
{
    return _cells[row * _cols + col];
}

TerminalCell& TerminalBuffer::_cell(int col, int row)
{
    return _cells[row * _cols + col];
}

void TerminalBuffer::_scrollUp()
{
    std::memmove(
        _cells.data(),
        _cells.data() + _cols,
        (_rows - 1) * _cols * sizeof(TerminalCell));

    for (int c = 0; c < _cols; ++c)
    {
        auto& cell = _cell(c, _rows - 1);
        cell.ch = L' ';
        cell.fgColor = 7;
        cell.bgColor = 0;
        cell.dirty = true;
    }

    if (_scrollCallback)
    {
        _scrollCallback();
    }
}

void TerminalBuffer::_ensureSize(int col, int row)
{
    if (col >= _cols) col = _cols - 1;
    if (row >= _rows) row = _rows - 1;
    if (col < 0) col = 0;
    if (row < 0) row = 0;
}

// --- VtParser ---

VtParser::VtParser(TerminalBuffer& buffer) :
    _buffer(buffer)
{
}

void VtParser::Parse(const char* data, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        char ch = data[i];

        switch (_state)
        {
        case State::Ground:
            _handleGround(ch);
            break;
        case State::EscapeStart:
            _handleEscape(ch);
            break;
        case State::EscapeIntermediate:
            _handleEscape(ch);
            break;
        case State::CsiEntry:
        case State::CsiParam:
            _handleCsi(ch);
            break;
        case State::OscString:
            _handleOsc(ch);
            break;
        }
    }
}

void VtParser::_handleGround(char ch)
{
    if (ch == '\x1b')
    {
        _state = State::EscapeStart;
        _paramString.clear();
        _intermediateChar = 0;
    }
    else if (ch == '\n' || ch == '\x0b' || ch == '\x0c')
    {
        _buffer.Newline();
    }
    else if (ch == '\r')
    {
        _buffer.CarriageReturn();
    }
    else if (ch == '\x08')
    {
        _buffer.Backspace();
    }
    else if (ch == '\x09')
    {
        _buffer.Tab();
    }
    else if (ch >= 0x20)
    {
        _buffer.PutChar(static_cast<wchar_t>(static_cast<unsigned char>(ch)));
    }
}

void VtParser::_handleEscape(char ch)
{
    if (ch == '[')
    {
        _state = State::CsiEntry;
        _paramString.clear();
        _intermediateChar = 0;
    }
    else if (ch == ']')
    {
        _state = State::OscString;
        _oscString.clear();
    }
    else if (ch == '(' || ch == ')')
    {
        _state = State::EscapeIntermediate;
        _intermediateChar = ch;
    }
    else
    {
        _state = State::Ground;
    }
}

void VtParser::_handleCsi(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        _paramString += ch;
        _state = State::CsiParam;
    }
    else if (ch == ';')
    {
        _paramString += ch;
        _state = State::CsiParam;
    }
    else if (ch == '?')
    {
        _paramString += ch;
        _state = State::CsiParam;
    }
    else if (ch >= 0x40 && ch <= 0x7e)
    {
        _executeCsi(ch);
        _state = State::Ground;
    }
    else
    {
        _state = State::Ground;
    }
}

void VtParser::_handleOsc(char ch)
{
    if (ch == '\x07')
    {
        _state = State::Ground;
    }
    else if (ch == '\x1b')
    {
        _state = State::Ground;
    }
    else
    {
        _oscString += ch;
    }
}

int VtParser::_getParam(int index, int defaultValue)
{
    std::string params = _paramString;
    int current = 0;
    int paramIndex = 0;
    std::string value;

    for (size_t i = 0; i < params.size(); ++i)
    {
        char ch = params[i];
        if (ch == ';')
        {
            if (paramIndex == index)
            {
                return value.empty() ? defaultValue : std::stoi(value);
            }
            paramIndex++;
            value.clear();
        }
        else if (ch >= '0' && ch <= '9')
        {
            value += ch;
        }
    }

    if (paramIndex == index)
    {
        return value.empty() ? defaultValue : std::stoi(value);
    }

    return defaultValue;
}

void VtParser::_executeCsi(char finalChar)
{
    std::string params = _paramString;
    bool hasQuestion = !params.empty() && params[0] == '?';
    if (hasQuestion)
    {
        params = params.substr(1);
    }

    switch (finalChar)
    {
    case 'A':
        _buffer.MoveCursorUp(_getParam(0, 1));
        break;
    case 'B':
        _buffer.MoveCursorDown(_getParam(0, 1));
        break;
    case 'C':
        _buffer.MoveCursorForward(_getParam(0, 1));
        break;
    case 'D':
        _buffer.MoveCursorBack(_getParam(0, 1));
        break;
    case 'E':
        _buffer.MoveCursorDown(_getParam(0, 1));
        _buffer.CarriageReturn();
        break;
    case 'F':
        _buffer.MoveCursorUp(_getParam(0, 1));
        _buffer.CarriageReturn();
        break;
    case 'G':
        _buffer.SetCursorPos(_getParam(0, 1) - 1, _buffer.GetCursorY());
        break;
    case 'H':
    case 'f':
    {
        int row = _getParam(0, 1);
        int col = _getParam(1, 1);
        _buffer.SetCursorPos(col - 1, row - 1);
        break;
    }
    case 'J':
        _buffer.EraseInDisplay(_getParam(0, 0));
        break;
    case 'K':
        _buffer.EraseInLine(_getParam(0, 0));
        break;
    case 'm':
    {
        std::string p = params;
        if (p.empty())
        {
            _buffer.ResetColors();
            break;
        }

        size_t pos = 0;
        while (pos <= p.size())
        {
            size_t semi = p.find(';', pos);
            std::string val = (semi == std::string::npos) ? p.substr(pos) : p.substr(pos, semi - pos);
            int code = val.empty() ? 0 : std::stoi(val);

            if (code == 0)
            {
                _buffer.ResetColors();
            }
            else if (code >= 30 && code <= 37)
            {
                _buffer.SetForegroundColor(ANSI_TO_VGA[code - 30]);
            }
            else if (code >= 40 && code <= 47)
            {
                _buffer.SetBackgroundColor(ANSI_TO_VGA[code - 40]);
            }
            else if (code == 39)
            {
                _buffer.SetForegroundColor(7);
            }
            else if (code == 49)
            {
                _buffer.SetBackgroundColor(0);
            }
            else if (code >= 90 && code <= 97)
            {
                _buffer.SetForegroundColor(ANSI_TO_VGA[code - 90]);
            }
            else if (code >= 100 && code <= 107)
            {
                _buffer.SetBackgroundColor(ANSI_TO_VGA[code - 100]);
            }

            if (semi == std::string::npos) break;
            pos = semi + 1;
        }
        break;
    }
    case 'S':
        for (int i = 0; i < _getParam(0, 1); ++i)
        {
            _buffer.EraseInLine(2);
            _buffer.Newline();
        }
        break;
    default:
        break;
    }
}
