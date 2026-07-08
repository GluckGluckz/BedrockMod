#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace pebble
{
    // Pattern scanner - similar to Qi Engine / Cheat Engine pattern finding
    class PebblePatternScanner
    {
    public:
        static std::vector<std::uintptr_t> FindAll(std::string_view pattern);
        
        // Find a specific function by name (optional string signature)
        static std::vector<std::uintptr_t> FindByName(const char* name, 
                                                      const char* optionalSig = nullptr);
    };

    enum class PatternType
    {
        BytePattern,  // "56 57 48 83 EC 28 ..."
        StringPattern // "FUNC_NAME" or "some_function_name"
    };
}
