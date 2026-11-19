#pragma once

#include <cstddef>
#include <cstdint>

namespace highlightchams
{
    enum class Status
    {
        Disabled,
        WaitingForGame,
        WaitingForCharacters,
        NoAvailableIds,
        MemoryUnavailable,
        Active
    };

    void Tick();
    void Run();
    void Stop();
    bool Active();
    Status GetStatus();
    std::size_t GetEntityCount();
    std::size_t GetPlayerCount();
    const char* StatusString();
}
