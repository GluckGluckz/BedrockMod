#pragma once

#include <atomic>
#include <chrono>

namespace pebble
{
    // Fires synthetic left-mouse clicks at a fixed rate while enabled and the
    // game holds foreground. Pure input simulation — no game memory required.
    class AutoClickerModule
    {
    public:
        static constexpr int kMinCps = 1;
        static constexpr int kMaxCps = 20;

        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        int ClicksPerSecond() const;
        void SetClicksPerSecond(int cps);
        void AdjustClicksPerSecond(int delta);

        // Called from the bootstrap tick. `gameActive` is true only when the
        // game window is foreground and the menu is closed.
        void Tick(bool gameActive);

    private:
        static void SendLeftClick();

        std::atomic<bool> enabled_{false};
        std::atomic<int> clicksPerSecond_{10};
        std::chrono::steady_clock::time_point lastClick_{};
    };
}
