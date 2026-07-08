#pragma once

// Example usage of the pattern scanner (Qi-style)
// 
// This is similar to what Cheat Engine or Qi Engine do:
// - Find functions by byte pattern: "56 57 48 83 EC 28 ..."
// - Read/write memory safely with proper types
// - Handle pointers and offsets

#include <cstdint>
#include <string_view>
#include <vector>

namespace pebble
{
    // Example: Find a function by pattern (like "LocalPlayerChanged")
    auto FindFunction(const char* name, const char* pattern) -> std::uintptr_t;

    // Example: Read a pointer from the game (safe wrapper)
    template<typename T>
    auto ReadPointer(void* base, size_t offset = 0) -> T*
    {
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + offset);
    }

    // Example: Write to memory safely
    template<typename T>
    void WritePointer(void* base, size_t offset, T* value)
    {
        auto* ptr = ReadPointer<T>(base, offset);
        if (ptr)
            *ptr = *value;
    }

    // Example: Find an entity by ID
    template<typename EntityIdType>
    void FindEntity(EntityIdType id)
    {
        // This would scan the entity registry for this ID
    }
}
