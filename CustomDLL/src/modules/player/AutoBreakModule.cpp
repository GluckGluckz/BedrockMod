#include "modules/player/AutoBreakModule.h"

#include <windows.h>

namespace pebble
{
    void AutoBreakModule::SetEnabled(bool enabled)
    {
        enabled_.store(enabled, std::memory_order_release);
    }

    bool AutoBreakModule::IsEnabled() const
    {
        return enabled_.load(std::memory_order_acquire);
    }

    void AutoBreakModule::Tick(bool gameActive)
    {
        const bool shouldHold = gameActive && enabled_.load(std::memory_order_acquire);
        if (!shouldHold)
        {
            if (buttonDown_)
            {
                SetButtonDown(false);
            }
            return;
        }

        // Re-assert the button-down periodically. A single press can be wasted
        // if it lands before the game has focus or while not aiming at a block;
        // re-pressing every ~200ms keeps the hold alive without releasing it.
        const auto now = std::chrono::steady_clock::now();
        if (!buttonDown_ || now - lastAssert_ >= std::chrono::milliseconds(200))
        {
            SetButtonDown(true);
            lastAssert_ = now;
        }
    }

    void AutoBreakModule::Stop()
    {
        if (buttonDown_)
        {
            SetButtonDown(false);
        }
    }

    void AutoBreakModule::SetButtonDown(bool down)
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
        buttonDown_ = down;
    }
}
