#pragma once

#include <cstdint>
#include <string>

namespace pebble
{
    struct GameBuild
    {
        std::uint32_t major = 0;
        std::uint32_t minor = 0;
        std::uint32_t patch = 0;
        std::uint32_t revision = 0;

        static GameBuild Detect();

        bool IsKnown() const;
        std::string ToString() const;
    };
}
