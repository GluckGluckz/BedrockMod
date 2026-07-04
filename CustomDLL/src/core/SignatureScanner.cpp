#include "core/SignatureScanner.h"

#include <sstream>
#include <string>
#include <windows.h>

namespace
{
    std::vector<int> ParsePattern(std::string_view pattern)
    {
        std::vector<int> bytes;
        std::istringstream stream{std::string(pattern)};
        std::string token;
        while (stream >> token)
        {
            if (token == "?" || token == "??")
            {
                bytes.push_back(-1);
            }
            else
            {
                bytes.push_back(std::stoi(token, nullptr, 16));
            }
        }
        return bytes;
    }
}

namespace pebble
{
    std::vector<std::uintptr_t> SignatureScanner::FindAll(std::string_view pattern)
    {
        std::vector<std::uintptr_t> matches;
        const HMODULE module = GetModuleHandleW(nullptr);
        if (module == nullptr)
        {
            return matches;
        }

        const auto* base = reinterpret_cast<const std::uint8_t*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        {
            return matches;
        }
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
        {
            return matches;
        }

        const auto bytes = ParsePattern(pattern);
        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section)
        {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
            {
                continue;
            }

            const auto* start = base + section->VirtualAddress;
            const std::size_t size = section->Misc.VirtualSize;
            if (size < bytes.size())
            {
                continue;
            }

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
                    matches.push_back(reinterpret_cast<std::uintptr_t>(start + offset));
                }
            }
        }
        return matches;
    }
}
