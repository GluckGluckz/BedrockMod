#pragma once

#include <atomic>
#include <chrono>

namespace pebble
{
    // Holds the left mouse button down while enabled and the game holds
    // foreground, producing continuous block breaking / mining. Pure input
    // simulation — no game memory required.
    class AutoBreakModule
    {
    public:
        void SetEnabled(bool enabled);
        bool IsEnabled() const;

        // Called from the bootstrap tick. `gameActive` is true only when the
        // game window is foreground and the menu is closed.
        void Tick(bool gameActive);
        void Stop();

    private:
        void SetButtonDown(bool down);

        std::atomic<bool> enabled_{false};
        bool buttonDown_ = false;
        std::chrono::steady_clock::time_point lastAssert_{};
    };
}
