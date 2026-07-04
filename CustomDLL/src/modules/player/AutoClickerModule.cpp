#include "modules/player/AutoClickerModule.h"

#include <algorithm>
#include <windows.h>

namespace pebble
{
    void AutoClickerModule::SetEnabled(bool enabled)
    {
        enabled_.store(enabled, std::memory_order_release);
        lastClick_ = {};
    }

    bool AutoClickerModule::IsEnabled() const
    {
        return enabled_.load(std::memory_order_acquire);
    }

    int AutoClickerModule::ClicksPerSecond() const
    {
        return clicksPerSecond_.load(std::memory_order_acquire);
    }

    void AutoClickerModule::SetClicksPerSecond(int cps)
    {
        cps = std::clamp(cps, kMinCps, kMaxCps);
        clicksPerSecond_.store(cps, std::memory_order_release);
    }

    void AutoClickerModule::AdjustClicksPerSecond(int delta)
    {
        SetClicksPerSecond(ClicksPerSecond() + delta);
    }

    void AutoClickerModule::Tick(bool gameActive)
    {
        if (!gameActive || !enabled_.load(std::memory_order_acquire))
        {
            lastClick_ = {};
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto interval =
            std::chrono::milliseconds(1000 / clicksPerSecond_.load(std::memory_order_acquire));

        // First activation: click immediately and anchor the schedule.
        if (lastClick_.time_since_epoch().count() == 0)
        {
            SendLeftClick();
            lastClick_ = now;
            return;
        }

        // The bootstrap tick is ~16ms, so a single click per tick caps out near
        // 60 CPS and drifts if ticks are late. Catch up by firing every whole
        // interval that has elapsed (bounded so we never spin).
        int budget = 8;
        while (now - lastClick_ >= interval && budget-- > 0)
        {
            SendLeftClick();
            lastClick_ += interval;
        }

        // If we fell far behind (e.g. after a long stall), resync to now.
        if (now - lastClick_ > interval * 4)
        {
            lastClick_ = now;
        }
    }

    void AutoClickerModule::SendLeftClick()
    {
        INPUT input[2]{};
        input[0].type = INPUT_MOUSE;
        input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        input[1].type = INPUT_MOUSE;
        input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, input, sizeof(INPUT));
    }
}
