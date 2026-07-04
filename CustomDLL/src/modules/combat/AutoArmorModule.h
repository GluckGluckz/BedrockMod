#pragma once

#include <atomic>

namespace pebble
{
    // Automatically equips the best available armor from the hotbar/inventory.
    // This requires reading the player's container contents and issuing
    // inventory transactions through Minecraft memory, which is not yet wired
    // (blocked on the same local-player/registry resolution as Night Vision).
    // The toggle latches so the UI is complete, but no action is taken yet.
    class AutoArmorModule
    {
    public:
        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        bool IsFunctional() const { return false; }

    private:
        std::atomic<bool> enabled_{false};
    };
}
