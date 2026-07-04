#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace pebble
{
    class SignatureScanner
    {
    public:
        static std::vector<std::uintptr_t> FindAll(std::string_view pattern);
    };
}
