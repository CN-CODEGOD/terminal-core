#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

class PtyBackend
{
public:
    using OutputCallback = std::function<void(const char*, size_t)>;

    PtyBackend();
    ~PtyBackend();

    bool Start(const std::wstring& command, int cols, int rows);
    void WriteInput(const char* data, size_t length);
    void Resize(int cols, int rows);
    void Stop();

    void SetOutputCallback(OutputCallback callback);
    bool IsRunning() const;

private:
    void _readOutput();

    HANDLE _hPty = INVALID_HANDLE_VALUE;
    HANDLE _hInputWrite = INVALID_HANDLE_VALUE;
    HANDLE _hOutputRead = INVALID_HANDLE_VALUE;
    HANDLE _hProcess = INVALID_HANDLE_VALUE;
    HANDLE _hThread = INVALID_HANDLE_VALUE;
    std::atomic<bool> _running{ false };
    OutputCallback _outputCallback;
};
