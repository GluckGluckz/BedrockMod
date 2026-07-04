#include "cosmetics/CosmeticRegistry.h"

#include <algorithm>
#include <utility>

namespace pebble
{
    bool CosmeticRegistry::Register(CosmeticDefinition cosmetic)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto existing = std::find_if(
            cosmetics_.begin(),
            cosmetics_.end(),
            [&cosmetic](const CosmeticDefinition& item) { return item.id == cosmetic.id; });
        if (existing != cosmetics_.end() || cosmetic.id.empty())
        {
            return false;
        }

        cosmetics_.push_back(std::move(cosmetic));
        return true;
    }

    bool CosmeticRegistry::SetEnabled(const std::string& id, bool enabled)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto selected = std::find_if(
            cosmetics_.begin(),
            cosmetics_.end(),
            [&id](const CosmeticDefinition& item) { return item.id == id; });
        if (selected == cosmetics_.end())
        {
            return false;
        }

        if (enabled)
        {
            for (auto& cosmetic : cosmetics_)
            {
                if (cosmetic.slot == selected->slot)
                {
                    cosmetic.enabled = false;
                }
            }
        }

        selected->enabled = enabled;
        return true;
    }

    bool CosmeticRegistry::IsEnabled(const std::string& id) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto cosmetic = std::find_if(
            cosmetics_.begin(),
            cosmetics_.end(),
            [&id](const CosmeticDefinition& item) { return item.id == id; });
        return cosmetic != cosmetics_.end() && cosmetic->enabled;
    }

    std::size_t CosmeticRegistry::Count() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return cosmetics_.size();
    }

    void CosmeticRegistry::Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cosmetics_.clear();
    }
}
