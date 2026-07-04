#include "modules/combat/TriggerbotModule.h"

namespace pebble
{
    void TriggerbotModule::SetEnabled(bool enabled)
    {
        enabled_.store(enabled, std::memory_order_release);
    }

    bool TriggerbotModule::IsEnabled() const
    {
        return enabled_.load(std::memory_order_acquire);
    }
}
