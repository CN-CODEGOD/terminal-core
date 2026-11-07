#include "PtyBackend.h"

PtyBackend::PtyBackend() = default;

PtyBackend::~PtyBackend()
{
    Stop();
}

bool PtyBackend::Start(const std::wstring& command, int cols, int rows)
{
    HANDLE hInputRead, hInputWrite;
    HANDLE hOutputRead, hOutputWrite;

    if (!CreatePipe(&hInputRead, &hInputWrite, nullptr, 0) ||
        !CreatePipe(&hOutputRead, &hOutputWrite, nullptr, 0))
    {
        return false;
    }

    HRESULT hr = CreatePseudoConsole(
        { static_cast<SHORT>(cols), static_cast<SHORT>(rows) },
        hInputRead,
        hOutputWrite,
        0,
        &_hPty);

    if (FAILED(hr))
    {
        CloseHandle(hInputRead);
        CloseHandle(hInputWrite);
        CloseHandle(hOutputRead);
        CloseHandle(hOutputWrite);
        return false;
    }

    _hInputWrite = hInputWrite;
    _hOutputRead = hOutputRead;

    CloseHandle(hInputRead);
    CloseHandle(hOutputWrite);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdError = INVALID_HANDLE_VALUE;
    si.StartupInfo.hStdInput = INVALID_HANDLE_VALUE;
    si.StartupInfo.hStdOutput = INVALID_HANDLE_VALUE;

    size_t attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);

    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(new BYTE[attrListSize]);

    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrListSize))
    {
        delete[] si.lpAttributeList;
        return false;
    }

    if (!UpdateProcThreadAttribute(
            si.lpAttributeList,
            0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            _hPty,
            sizeof(HANDLE),
            nullptr,
            nullptr))
    {
        DeleteProcThreadAttributeList(si.lpAttributeList);
        delete[] si.lpAttributeList;
        return false;
    }

    std::wstring cmdLine = command.empty() ? L"cmd.exe" : command;

    PROCESS_INFORMATION pi{};
    BOOL success = CreateProcessW(
        nullptr,
        &cmdLine[0],
        nullptr,
        nullptr,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT,
        nullptr,
        nullptr,
        &si.StartupInfo,
        &pi);

    DeleteProcThreadAttributeList(si.lpAttributeList);
    delete[] si.lpAttributeList;

    if (!success)
    {
        return false;
    }

    _hProcess = pi.hProcess;
    _hThread = pi.hThread;
    _running = true;

    std::thread([this]() { _readOutput(); }).detach();

    return true;
}

void PtyBackend::_readOutput()
{
    char buffer[4096];
    DWORD bytesRead;

    while (_running && ReadFile(_hOutputRead, buffer, sizeof(buffer), &bytesRead, nullptr))
    {
        if (bytesRead > 0 && _outputCallback)
        {
            _outputCallback(buffer, bytesRead);
        }
    }
}

void PtyBackend::WriteInput(const char* data, size_t length)
{
    if (_hInputWrite != INVALID_HANDLE_VALUE && _running)
    {
        DWORD written;
        WriteFile(_hInputWrite, data, static_cast<DWORD>(length), &written, nullptr);
    }
}

void PtyBackend::Resize(int cols, int rows)
{
    if (_hPty != INVALID_HANDLE_VALUE)
    {
        ResizePseudoConsole(_hPty, { static_cast<SHORT>(cols), static_cast<SHORT>(rows) });
    }
}

void PtyBackend::Stop()
{
    _running = false;

    if (_hProcess != INVALID_HANDLE_VALUE)
    {
        TerminateProcess(_hProcess, 0);
        CloseHandle(_hProcess);
        _hProcess = INVALID_HANDLE_VALUE;
    }

    if (_hThread != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_hThread);
        _hThread = INVALID_HANDLE_VALUE;
    }

    if (_hPty != INVALID_HANDLE_VALUE)
    {
        ClosePseudoConsole(_hPty);
        _hPty = INVALID_HANDLE_VALUE;
    }

    if (_hInputWrite != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_hInputWrite);
        _hInputWrite = INVALID_HANDLE_VALUE;
    }

    if (_hOutputRead != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_hOutputRead);
        _hOutputRead = INVALID_HANDLE_VALUE;
    }
}

void PtyBackend::SetOutputCallback(OutputCallback callback)
{
    _outputCallback = std::move(callback);
}

bool PtyBackend::IsRunning() const
{
    return _running;
}
