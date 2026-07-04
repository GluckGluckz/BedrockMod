#include "core/GameBuild.h"

#include <array>
#include <cwchar>
#include <sstream>
#include <windows.h>

namespace
{
    constexpr wchar_t kPackageMarker[] = L"Microsoft.MinecraftUWP_";

    bool ParseComponent(const wchar_t*& cursor, std::uint32_t& value)
    {
        if (*cursor < L'0' || *cursor > L'9')
        {
            return false;
        }

        value = 0;
        while (*cursor >= L'0' && *cursor <= L'9')
        {
            value = value * 10 + static_cast<std::uint32_t>(*cursor - L'0');
            ++cursor;
        }
        return true;
    }
}

namespace pebble
{
    GameBuild GameBuild::Detect()
    {
        std::array<wchar_t, 32768> path{};
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size())
        {
            return {};
        }

        const wchar_t* cursor = std::wcsstr(path.data(), kPackageMarker);
        if (cursor == nullptr)
        {
            return {};
        }
        cursor += std::size(kPackageMarker) - 1;

        GameBuild build;
        std::uint32_t* components[] = {
            &build.major,
            &build.minor,
            &build.patch,
            &build.revision
        };

        for (std::size_t index = 0; index < std::size(components); ++index)
        {
            if (!ParseComponent(cursor, *components[index]))
            {
                return {};
            }
            if (index + 1 < std::size(components) && *cursor++ != L'.')
            {
                return {};
            }
        }

        return build;
    }

    bool GameBuild::IsKnown() const
    {
        return major != 0 || minor != 0 || patch != 0 || revision != 0;
    }

    std::string GameBuild::ToString() const
    {
        if (!IsKnown())
        {
            return "unknown";
        }

        std::ostringstream stream;
        stream << major << '.' << minor << '.' << patch << '.' << revision;
        return stream.str();
    }
}
