#include "SummonManager.h"

SummonManager::SummonManager() = default;

SummonManager::~SummonManager()
{
    Shutdown();
}

bool SummonManager::Initialize(HWND messageWindow)
{
    _messageWindow = messageWindow;
    return true;
}

void SummonManager::Shutdown()
{
    RemoveAllHotkeys();
    _messageWindow = nullptr;
}

void SummonManager::AddSummonHotkey(UINT modifiers, UINT vk, std::function<void()> callback)
{
    int id = _nextHotkeyId++;

    if (::RegisterHotKey(_messageWindow, id, modifiers, vk))
    {
        HotkeyBinding binding;
        binding.modifiers = modifiers;
        binding.vk = vk;
        binding.id = id;
        binding.callback = std::move(callback);
        _hotkeys.push_back(std::move(binding));
    }
}

void SummonManager::RemoveAllHotkeys()
{
    for (const auto& binding : _hotkeys)
    {
        ::UnregisterHotKey(_messageWindow, binding.id);
    }
    _hotkeys.clear();
    _nextHotkeyId = 200;
}

void SummonManager::HandleHotkey(int hotkeyId)
{
    for (const auto& binding : _hotkeys)
    {
        if (binding.id == hotkeyId)
        {
            if (binding.callback)
            {
                binding.callback();
            }
            return;
        }
    }
}
