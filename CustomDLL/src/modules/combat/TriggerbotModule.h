#pragma once

#include <atomic>

namespace pebble
{
    // Attacks automatically when the crosshair is over a hostile entity.
    // Deciding whether an entity is targeted requires reading Minecraft's
    // hit-test / raycast result from memory, which is not yet wired (blocked on
    // the same local-player/registry resolution as Night Vision). The toggle
    // latches so the UI is complete, but no attack is issued yet.
    class TriggerbotModule
    {
    public:
        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        bool IsFunctional() const { return false; }

    private:
        std::atomic<bool> enabled_{false};
    };
}
