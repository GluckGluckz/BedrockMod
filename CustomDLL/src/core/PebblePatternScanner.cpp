#include "core/PebblePatternScanner.h"

#include <sstream>
#include <string>
#include <windows.h>

namespace pebble
{
    std::vector<std::uintptr_t> PebblePatternScanner::FindAll(std::string_view pattern)
    {
        // This is essentially what your SignatureScanner already does!
        // It finds byte patterns in the game binary.
        
        const HMODULE module = GetModuleHandleW(nullptr);
        if (module == nullptr)
            return {};

        const auto* base = reinterpret_cast<const std::uint8_t*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return {};

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return {};

        // Parse the pattern (handles "56 57 ?" format and wildcards)
        std::vector<int> bytes;
        std::istringstream stream{std::string(pattern)};
        std::string token;
        while (stream >> token)
        {
            if (token == "?" || token == "??" || token == "??")
                bytes.push_back(-1);  // wildcard
            else
                bytes.push_back(std::stoi(token, nullptr, 16));
        }

        // Scan executable sections for matches
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
                continue;

            const auto* start = base + section->VirtualAddress;
            std::size_t size = section->Misc.VirtualSize;
            
            for (std::size_t offset = 0; offset <= size - bytes.size(); ++offset)
            {
                bool matched = true;
                for (std::size_t byte = 0; byte < bytes.size(); ++byte)
                {
                    if (bytes[byte] >= 0 && start[offset + byte] != bytes[byte])
                    {
                        matched = false;
                        break;
                    }
                }
                
                if (matched)
                {
                    return {reinterpret_cast<std::uintptr_t>(start + offset)};
                }
            }
        }

        return {};
    }

    std::vector<std::uintptr_t> PebblePatternScanner::FindByName(const char* name, 
                                                                   const char* optionalSig)
    {
        // Optional: search by function name (PEB/PEX export table or debug symbols)
        // This is more like Cheat Engine's "Name" filter
        
        std::vector<std::uintptr_t> matches;
        
        // For now, just return empty - you'd need to scan the PEB for exports
        // or use a library like ImagePep for name-based searching
        
        return matches;
    }
}
