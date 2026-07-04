#include "modules/combat/AutoArmorModule.h"

namespace pebble
{
    void AutoArmorModule::SetEnabled(bool enabled)
    {
        enabled_.store(enabled, std::memory_order_release);
    }

    bool AutoArmorModule::IsEnabled() const
    {
        return enabled_.load(std::memory_order_acquire);
    }
}
