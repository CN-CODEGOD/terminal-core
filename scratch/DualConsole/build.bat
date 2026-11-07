@echo off
setlocal

set BUILD_DIR=build
set CONFIG=Release

if "%1"=="debug" set CONFIG=Debug
if "%1"=="clean" (
    if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
    echo Cleaned build directory.
    goto :eof
)

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo Configuring with CMake (%CONFIG%)...
cmake -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64

if errorlevel 1 (
    echo CMake configuration failed.
    exit /b 1
)

echo Building...
cmake --build %BUILD_DIR% --config %CONFIG%

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
echo Build successful!
echo Output: %BUILD_DIR%\%CONFIG%\DualConsole.exe
echo.
echo Usage:
echo   Ctrl+`          Toggle Window Mode
echo   Ctrl+Shift+`    Toggle Quake Mode
echo   Alt+`           Switch between modes

endlocal
