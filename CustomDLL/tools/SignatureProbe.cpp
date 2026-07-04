#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>

namespace
{
    struct Pattern
    {
        const char* name;
        const char* text;
    };

    std::vector<int> Parse(std::string_view pattern)
    {
        std::vector<int> bytes;
        std::istringstream stream{std::string(pattern)};
        std::string token;
        while (stream >> token)
        {
            bytes.push_back(token == "?" || token == "??"
                ? -1
                : std::stoi(token, nullptr, 16));
        }
        return bytes;
    }

    bool IsExecutable(DWORD protection)
    {
        protection &= 0xFF;
        return protection == PAGE_EXECUTE ||
            protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE ||
            protection == PAGE_EXECUTE_WRITECOPY;
    }

    DWORD FindMinecraftProcess()
    {
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W entry{sizeof(entry)};
        DWORD result = 0;
        if (snapshot != INVALID_HANDLE_VALUE && Process32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, L"Minecraft.Windows.exe") == 0)
                {
                    result = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        if (snapshot != INVALID_HANDLE_VALUE) CloseHandle(snapshot);
        return result;
    }

    bool FindMainModule(DWORD processId, std::uintptr_t& base, std::size_t& size)
    {
        const HANDLE snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
            processId);
        MODULEENTRY32W entry{sizeof(entry)};
        bool found = false;
        if (snapshot != INVALID_HANDLE_VALUE && Module32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szModule, L"Minecraft.Windows.exe") == 0)
                {
                    base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                    size = entry.modBaseSize;
                    found = true;
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }
        if (snapshot != INVALID_HANDLE_VALUE) CloseHandle(snapshot);
        return found;
    }
}

int wmain()
{
    std::cout << std::unitbuf;
    constexpr Pattern patterns[] = {
        {
            "LocalPlayerChangedConnector callback",
            "56 57 48 83 EC 28 48 89 CE 48 8B 89 80 00 00 00 "
            "48 85 C9 74 ? 48 8B 01 48 8B 40 10 48 8B 3A "
            "FF 15 ? ? ? ? 48 85 FF 74 ? 48 8B 4E 40"
        }
    };

    const DWORD processId = FindMinecraftProcess();
    if (processId == 0)
    {
        std::cerr << "Minecraft.Windows.exe is not running.\n";
        return 1;
    }

    std::uintptr_t moduleBase = 0;
    std::size_t moduleSize = 0;
    if (!FindMainModule(processId, moduleBase, moduleSize))
    {
        std::cerr << "Could not locate the Minecraft main module.\n";
        return 2;
    }

    const HANDLE process = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE,
        processId);
    if (process == nullptr)
    {
        std::cerr << "OpenProcess failed with " << GetLastError() << ".\n";
        return 3;
    }

    std::vector<std::vector<std::uintptr_t>> matches(std::size(patterns));
    constexpr std::string_view rttiNames[] = {
        ".?AVLocalPlayer@@",
        ".?AVClientInstance@@",
        ".?AVMobEffectsComponent@@",
        "LocalPlayer",
        "ClientInstance",
        "MobEffectsComponent",
        "HudMobEffectsRenderer",
        "findPrimaryLocalPlayer",
        "mGetLocalPlayer"
    };
    std::vector<std::vector<std::uintptr_t>> rttiMatches(std::size(rttiNames));
    std::vector<std::vector<int>> parsed;
    parsed.reserve(std::size(patterns));
    for (const auto& pattern : patterns) parsed.push_back(Parse(pattern.text));

    std::uintptr_t cursor = moduleBase;
    const std::uintptr_t end = moduleBase + moduleSize;
    while (cursor < end)
    {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQueryEx(
                process,
                reinterpret_cast<void*>(cursor),
                &region,
                sizeof(region)) == 0)
        {
            break;
        }

        const auto regionBase = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        const std::size_t regionSize = std::min<std::size_t>(
            region.RegionSize,
            end - regionBase);
        if (region.State == MEM_COMMIT && IsExecutable(region.Protect) &&
            (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0)
        {
            std::vector<std::uint8_t> buffer(regionSize);
            SIZE_T bytesRead = 0;
            if (ReadProcessMemory(
                    process,
                    region.BaseAddress,
                    buffer.data(),
                    buffer.size(),
                    &bytesRead))
            {
                for (std::size_t patternIndex = 0; patternIndex < parsed.size(); ++patternIndex)
                {
                    const auto& bytes = parsed[patternIndex];
                    if (bytesRead < bytes.size()) continue;
                    for (std::size_t offset = 0; offset <= bytesRead - bytes.size(); ++offset)
                    {
                        bool matched = true;
                        for (std::size_t index = 0; index < bytes.size(); ++index)
                        {
                            if (bytes[index] >= 0 && buffer[offset + index] != bytes[index])
                            {
                                matched = false;
                                break;
                            }
                        }
                        if (matched)
                        {
                            matches[patternIndex].push_back(regionBase + offset - moduleBase);
                        }
                    }
                }
            }
        }
        if (region.State == MEM_COMMIT &&
            (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 &&
            regionBase >= moduleBase && regionBase < end)
        {
            std::vector<std::uint8_t> buffer(regionSize);
            SIZE_T bytesRead = 0;
            if (ReadProcessMemory(
                    process,
                    region.BaseAddress,
                    buffer.data(),
                    buffer.size(),
                    &bytesRead))
            {
                for (std::size_t nameIndex = 0; nameIndex < std::size(rttiNames); ++nameIndex)
                {
                    const auto name = rttiNames[nameIndex];
                    const auto found = std::search(
                        buffer.begin(),
                        buffer.begin() + bytesRead,
                        name.begin(),
                        name.end());
                    if (found != buffer.begin() + bytesRead)
                    {
                        rttiMatches[nameIndex].push_back(
                            regionBase + static_cast<std::size_t>(found - buffer.begin()) - moduleBase);
                    }
                }
            }
        }
        cursor = regionBase + region.RegionSize;
    }
    std::cout << "pid=" << processId
              << " base=0x" << std::hex << moduleBase
              << " size=0x" << moduleSize << std::dec << '\n';
    for (std::size_t index = 0; index < std::size(patterns); ++index)
    {
        std::cout << patterns[index].name << " matches=" << matches[index].size();
        const std::size_t displayed = std::min<std::size_t>(matches[index].size(), 20);
        for (std::size_t matchIndex = 0; matchIndex < displayed; ++matchIndex)
        {
            const auto offset = matches[index][matchIndex];
            std::cout << " rva=0x" << std::hex << offset << std::dec;
        }
        if (matches[index].size() > displayed) std::cout << " ...";
        std::cout << '\n';
        if (index >= 3)
        {
            for (const auto offset : rttiMatches[index])
            {
                char context[161]{};
                SIZE_T bytesRead = 0;
                const auto address = moduleBase + offset;
                ReadProcessMemory(
                    process,
                    reinterpret_cast<void*>(address > 64 ? address - 64 : address),
                    context,
                    160,
                    &bytesRead);
                for (SIZE_T character = 0; character < bytesRead; ++character)
                {
                    const unsigned char value = context[character];
                    if (value < 32 || value > 126) context[character] = '.';
                }
                std::cout << "  context: " << context << '\n';
            }
        }
    }
    CloseHandle(process);
    for (std::size_t index = 0; index < std::size(rttiNames); ++index)
    {
        std::cout << "RTTI " << rttiNames[index] << " matches=" << rttiMatches[index].size();
        for (const auto offset : rttiMatches[index])
        {
            std::cout << " rva=0x" << std::hex << offset << std::dec;
        }
        std::cout << '\n';
    }
    return 0;
}
