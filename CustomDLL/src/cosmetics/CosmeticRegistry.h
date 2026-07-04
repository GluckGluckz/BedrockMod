#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace pebble
{
    enum class CosmeticSlot
    {
        Cape,
        Back,
        Head,
        Aura
    };

    struct CosmeticDefinition
    {
        std::string id;
        std::string displayName;
        CosmeticSlot slot;
        bool enabled = false;
    };

    class CosmeticRegistry
    {
    public:
        bool Register(CosmeticDefinition cosmetic);
        bool SetEnabled(const std::string& id, bool enabled);
        bool IsEnabled(const std::string& id) const;
        std::size_t Count() const;
        void Clear();

    private:
        mutable std::mutex mutex_;
        std::vector<CosmeticDefinition> cosmetics_;
    };
}
