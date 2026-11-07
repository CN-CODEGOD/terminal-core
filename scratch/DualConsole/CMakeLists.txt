cmake_minimum_required(VERSION 3.20)
project(DualConsole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DualConsole WIN32
    main.cpp
    PtyBackend.cpp
    PtyBackend.h
    TerminalWindow.cpp
    TerminalWindow.h
    TerminalBuffer.cpp
    TerminalBuffer.h
    SummonManager.cpp
    SummonManager.h
)

target_link_libraries(DualConsole PRIVATE
    user32
    gdi32
    dwmapi
)

target_compile_definitions(DualConsole PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(DualConsole PRIVATE /utf-8 /W4)
endif()
