#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <windows.h>

namespace
{
    template <typename T>
    T GetExport(HMODULE module, const char* name)
    {
        return reinterpret_cast<T>(GetProcAddress(module, name));
    }
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Expected the PebbleCore DLL path.\n";
        return 1;
    }

    const std::wstring dllPath = argv[1];
    const HMODULE module = LoadLibraryW(dllPath.c_str());
    if (module == nullptr)
    {
        std::cerr << "LoadLibraryW failed with error " << GetLastError() << ".\n";
        return 2;
    }

    using IsActiveFn = BOOL (*)();
    using GetNameFn = const char* (*)();
    using GetApiVersionFn = std::uint32_t (*)();
    using GetStringFn = const char* (*)();
    using GetUint32Fn = std::uint32_t (*)();
    using SetCosmeticFn = BOOL (*)(const char*, BOOL);
    using IsCosmeticEnabledFn = BOOL (*)(const char*);
    using SetBoolFn = BOOL (*)(BOOL);
    using ShutdownFn = BOOL (*)();

    const auto isActive = GetExport<IsActiveFn>(module, "IsModActive");
    const auto getName = GetExport<GetNameFn>(module, "GetModName");
    const auto getApiVersion = GetExport<GetApiVersionFn>(module, "GetModApiVersion");
    const auto getGameVersion = GetExport<GetStringFn>(module, "GetDetectedGameVersion");
    const auto getHookStatus = GetExport<GetUint32Fn>(module, "GetRenderHookStatus");
    const auto getCosmeticCount = GetExport<GetUint32Fn>(module, "GetCosmeticCount");
    const auto setCosmeticEnabled =
        GetExport<SetCosmeticFn>(module, "SetCosmeticEnabled");
    const auto isCosmeticEnabled =
        GetExport<IsCosmeticEnabledFn>(module, "IsCosmeticEnabled");
    const auto isMenuVisible = GetExport<IsActiveFn>(module, "IsMenuVisible");
    const auto toggleMenu = GetExport<IsActiveFn>(module, "ToggleMenu");
    const auto isZoomEnabled = GetExport<IsActiveFn>(module, "IsZoomEnabled");
    const auto setZoomEnabled = GetExport<SetBoolFn>(module, "SetZoomEnabled");
    const auto isZoomActive = GetExport<IsActiveFn>(module, "IsZoomActive");
    const auto isZoomKeybindEnabled =
        GetExport<IsActiveFn>(module, "IsZoomKeybindEnabled");
    const auto setZoomKeybindEnabled =
        GetExport<SetBoolFn>(module, "SetZoomKeybindEnabled");
    const auto isNightVisionAvailable =
        GetExport<IsActiveFn>(module, "IsNightVisionAvailable");
    const auto isNightVisionEnabled =
        GetExport<IsActiveFn>(module, "IsNightVisionEnabled");
    const auto setNightVisionEnabled =
        GetExport<SetBoolFn>(module, "SetNightVisionEnabled");
    const auto isNightVisionApplied =
        GetExport<IsActiveFn>(module, "IsNightVisionApplied");
    const auto shutdown = GetExport<ShutdownFn>(module, "RequestShutdown");
    if (!isActive || !getName || !getApiVersion || !getGameVersion ||
        !getHookStatus || !getCosmeticCount || !setCosmeticEnabled ||
        !isCosmeticEnabled || !isMenuVisible || !toggleMenu ||
        !isZoomEnabled || !setZoomEnabled || !isZoomActive ||
        !isZoomKeybindEnabled || !setZoomKeybindEnabled ||
        !isNightVisionAvailable || !isNightVisionEnabled ||
        !setNightVisionEnabled || !isNightVisionApplied || !shutdown)
    {
        std::cerr << "One or more required exports are missing.\n";
        FreeLibrary(module);
        return 3;
    }

    for (int attempt = 0; attempt < 100 && !isActive(); ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!isActive())
    {
        std::cerr << "PebbleCore did not initialize.\n";
        FreeLibrary(module);
        return 4;
    }

    if (getCosmeticCount() != 3)
    {
        std::cerr << "Unexpected cosmetics registry size.\n";
        FreeLibrary(module);
        return 5;
    }

    constexpr char kFounderCape[] = "pebblecore:founder_cape";
    if (!setCosmeticEnabled(kFounderCape, TRUE) || !isCosmeticEnabled(kFounderCape))
    {
        std::cerr << "Cosmetic enable state did not round-trip.\n";
        FreeLibrary(module);
        return 6;
    }
    if (isMenuVisible())
    {
        std::cerr << "Client menu should start hidden.\n";
        FreeLibrary(module);
        return 7;
    }
    if (isZoomEnabled() || isZoomActive() || !isZoomKeybindEnabled())
    {
        std::cerr << "Zoom defaults are incorrect.\n";
        FreeLibrary(module);
        return 8;
    }
    if (isNightVisionAvailable() || isNightVisionEnabled() ||
        isNightVisionApplied() || setNightVisionEnabled(TRUE))
    {
        std::cerr << "Night Vision did not fail closed outside Minecraft.\n";
        FreeLibrary(module);
        return 9;
    }
    if (!setZoomKeybindEnabled(FALSE) || isZoomKeybindEnabled() ||
        !setZoomKeybindEnabled(TRUE) || !isZoomKeybindEnabled() ||
        !setZoomEnabled(TRUE) || !isZoomEnabled() ||
        !setZoomEnabled(FALSE) || isZoomEnabled())
    {
        std::cerr << "Zoom settings did not round-trip.\n";
        FreeLibrary(module);
        return 10;
    }
    if (!toggleMenu())
    {
        std::cerr << "Client menu rejected the toggle request.\n";
        FreeLibrary(module);
        return 11;
    }
    for (int attempt = 0; attempt < 50 && !isMenuVisible(); ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!isMenuVisible())
    {
        std::cerr << "Client menu did not become visible.\n";
        FreeLibrary(module);
        return 12;
    }
    toggleMenu();
    for (int attempt = 0; attempt < 50 && isMenuVisible(); ++attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (isMenuVisible())
    {
        std::cerr << "Client menu did not close.\n";
        FreeLibrary(module);
        return 13;
    }

    std::cout
        << getName() << " API v" << getApiVersion()
        << " loaded; game=" << getGameVersion()
        << ", hookStatus=" << getHookStatus()
        << ", cosmetics=" << getCosmeticCount() << ".\n";
    if (!shutdown())
    {
        std::cerr << "PebbleCore rejected the shutdown request.\n";
        FreeLibrary(module);
        return 14;
    }

    // RequestShutdown transfers the final FreeLibrary call to PebbleCore's
    // bootstrap thread. Give it time to complete before this host exits.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}
