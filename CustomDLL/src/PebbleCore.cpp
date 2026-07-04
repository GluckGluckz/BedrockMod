#include "PebbleCore.h"

#include "core/GameBuild.h"
#include "cosmetics/CosmeticRegistry.h"
#include "hooks/D3D12RenderHook.h"
#include "modules/render/ZoomModule.h"
#include "modules/render/NightVisionModule.h"
#include "modules/combat/AutoArmorModule.h"
#include "modules/combat/TriggerbotModule.h"
#include "modules/player/AutoClickerModule.h"
#include "modules/player/AutoBreakModule.h"
#include "ui/ClientMenu.h"

#include <atomic>
#include <cwchar>
#include <memory>
#include <string>

namespace
{
    constexpr std::uint32_t kModApiVersion = 1;

    std::atomic<bool> g_initialized{false};
    std::atomic<HANDLE> g_stopEvent{nullptr};
    HMODULE g_module = nullptr;
    pebble::GameBuild g_gameBuild;
    std::string g_gameVersion = "unknown";
    pebble::CosmeticRegistry g_cosmetics;
    std::unique_ptr<pebble::D3D12RenderHook> g_renderHook;
    std::unique_ptr<pebble::ZoomModule> g_zoomModule;
    std::unique_ptr<pebble::NightVisionModule> g_nightVisionModule;
    std::unique_ptr<pebble::AutoArmorModule> g_autoArmorModule;
    std::unique_ptr<pebble::TriggerbotModule> g_triggerbotModule;
    std::unique_ptr<pebble::AutoClickerModule> g_autoClickerModule;
    std::unique_ptr<pebble::AutoBreakModule> g_autoBreakModule;
    std::unique_ptr<pebble::ClientMenu> g_clientMenu;

    bool InitializeModules()
    {
        g_gameBuild = pebble::GameBuild::Detect();
        g_gameVersion = g_gameBuild.ToString();

        g_cosmetics.Register(
            {"pebblecore:founder_cape", "Founder Cape", pebble::CosmeticSlot::Cape});
        g_cosmetics.Register(
            {"pebblecore:pebble_pack", "Pebble Pack", pebble::CosmeticSlot::Back});
        g_cosmetics.Register(
            {"pebblecore:quest_crown", "Quest Crown", pebble::CosmeticSlot::Head});

        g_renderHook = std::make_unique<pebble::D3D12RenderHook>(g_gameBuild);
        if (!g_renderHook->Start())
        {
            g_renderHook.reset();
            g_cosmetics.Clear();
            return false;
        }

        g_zoomModule = std::make_unique<pebble::ZoomModule>();
        if (!g_zoomModule->Start(g_module))
        {
            OutputDebugStringA("PebbleCore: Zoom visual surface unavailable\n");
        }

        g_nightVisionModule = std::make_unique<pebble::NightVisionModule>(g_gameBuild);
        if (!g_nightVisionModule->Start())
        {
            OutputDebugStringA("PebbleCore: Night Vision memory hook unavailable\n");
        }

        g_autoArmorModule = std::make_unique<pebble::AutoArmorModule>();
        g_triggerbotModule = std::make_unique<pebble::TriggerbotModule>();
        g_autoClickerModule = std::make_unique<pebble::AutoClickerModule>();
        g_autoBreakModule = std::make_unique<pebble::AutoBreakModule>();

        g_clientMenu = std::make_unique<pebble::ClientMenu>(
            g_cosmetics,
            *g_zoomModule,
            *g_nightVisionModule,
            *g_autoArmorModule,
            *g_triggerbotModule,
            *g_autoClickerModule,
            *g_autoBreakModule);
        if (!g_clientMenu->Start(g_module))
        {
            OutputDebugStringA("PebbleCore: client menu could not be created\n");
            g_clientMenu.reset();
        }

        OutputDebugStringA("PebbleCore: module initialization complete\n");
        return true;
    }

    void CleanupModules()
    {
        if (g_clientMenu)
        {
            g_clientMenu->Stop();
            g_clientMenu.reset();
        }
        if (g_zoomModule)
        {
            g_zoomModule->Stop();
            g_zoomModule.reset();
        }
        if (g_nightVisionModule)
        {
            g_nightVisionModule->Stop();
            g_nightVisionModule.reset();
        }
        if (g_autoBreakModule)
        {
            g_autoBreakModule->Stop();
            g_autoBreakModule.reset();
        }
        g_autoClickerModule.reset();
        g_triggerbotModule.reset();
        g_autoArmorModule.reset();
        if (g_renderHook)
        {
            g_renderHook->Stop();
            g_renderHook.reset();
        }
        g_cosmetics.Clear();
        OutputDebugStringA("PebbleCore: module cleanup complete\n");
    }

    BOOL CALLBACK FindGameWindowProc(HWND window, LPARAM parameter)
    {
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId != GetCurrentProcessId() || !IsWindowVisible(window))
        {
            return TRUE;
        }

        // Skip our own overlay windows (menu, zoom host) so they never count as
        // "the game" — that was the bug that let AutoBreak stick while an
        // overlay held focus.
        wchar_t className[64]{};
        GetClassNameW(window, className, 64);
        if (wcsncmp(className, L"PebbleCore.", 11) == 0)
        {
            return TRUE;
        }

        RECT bounds{};
        if (!GetWindowRect(window, &bounds) ||
            bounds.right - bounds.left < 400 || bounds.bottom - bounds.top < 300)
        {
            return TRUE;
        }

        *reinterpret_cast<HWND*>(parameter) = window;
        return FALSE;
    }

    HWND FindGameWindow()
    {
        HWND result = nullptr;
        EnumWindows(FindGameWindowProc, reinterpret_cast<LPARAM>(&result));
        return result;
    }

    DWORD WINAPI BootstrapThread(void*)
    {
        const HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (stopEvent == nullptr)
        {
            OutputDebugStringA("PebbleCore: failed to create shutdown event\n");
            FreeLibraryAndExitThread(g_module, 1);
        }

        g_stopEvent.store(stopEvent, std::memory_order_release);

        if (!InitializeModules())
        {
            g_stopEvent.store(nullptr, std::memory_order_release);
            CloseHandle(stopEvent);
            FreeLibraryAndExitThread(g_module, 1);
        }

        g_initialized.store(true, std::memory_order_release);
        OutputDebugStringA("PebbleCore: loaded successfully\n");

        while (WaitForSingleObject(stopEvent, 16) == WAIT_TIMEOUT)
        {
            if (g_zoomModule)
            {
                g_zoomModule->Tick();
            }
            if (g_clientMenu)
            {
                g_clientMenu->Tick();
            }

            // Input-simulation modules only act when the actual Minecraft game
            // window is the foreground window and the menu is closed, so input
            // can never land on our overlays or another application. (Cache the
            // game window and refresh only if it becomes invalid.)
            static HWND s_gameWindow = nullptr;
            if (s_gameWindow == nullptr || !IsWindow(s_gameWindow))
            {
                s_gameWindow = FindGameWindow();
            }
            const bool menuVisible = g_clientMenu && g_clientMenu->IsVisible();
            const bool gameActive =
                !menuVisible && s_gameWindow != nullptr &&
                GetForegroundWindow() == s_gameWindow;
            if (g_autoClickerModule)
            {
                g_autoClickerModule->Tick(gameActive);
            }
            if (g_autoBreakModule)
            {
                g_autoBreakModule->Tick(gameActive);
            }
        }

        g_initialized.store(false, std::memory_order_release);
        CleanupModules();
        g_stopEvent.store(nullptr, std::memory_order_release);
        CloseHandle(stopEvent);
        FreeLibraryAndExitThread(g_module, 0);
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = module;
        DisableThreadLibraryCalls(module);

        const HANDLE thread = CreateThread(
            nullptr,
            0,
            BootstrapThread,
            nullptr,
            0,
            nullptr);
        if (thread == nullptr)
        {
            return FALSE;
        }

        CloseHandle(thread);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        g_initialized.store(false, std::memory_order_release);
    }

    return TRUE;
}

BOOL IsModActive()
{
    return g_initialized.load(std::memory_order_acquire) ? TRUE : FALSE;
}

const char* GetModName()
{
    return "PebbleCore";
}

std::uint32_t GetModApiVersion()
{
    return kModApiVersion;
}

BOOL RequestShutdown()
{
    const HANDLE stopEvent = g_stopEvent.load(std::memory_order_acquire);
    return stopEvent != nullptr && SetEvent(stopEvent) ? TRUE : FALSE;
}

const char* GetDetectedGameVersion()
{
    return g_gameVersion.c_str();
}

std::uint32_t GetRenderHookStatus()
{
    return g_renderHook
        ? static_cast<std::uint32_t>(g_renderHook->Status())
        : static_cast<std::uint32_t>(pebble::HookStatus::NotStarted);
}

std::uint32_t GetCosmeticCount()
{
    return static_cast<std::uint32_t>(g_cosmetics.Count());
}

BOOL SetCosmeticEnabled(const char* cosmeticId, BOOL enabled)
{
    if (cosmeticId == nullptr)
    {
        return FALSE;
    }
    return g_cosmetics.SetEnabled(cosmeticId, enabled != FALSE) ? TRUE : FALSE;
}

BOOL IsCosmeticEnabled(const char* cosmeticId)
{
    if (cosmeticId == nullptr)
    {
        return FALSE;
    }
    return g_cosmetics.IsEnabled(cosmeticId) ? TRUE : FALSE;
}

BOOL IsMenuVisible()
{
    return g_clientMenu && g_clientMenu->IsVisible() ? TRUE : FALSE;
}

BOOL ToggleMenu()
{
    if (!g_clientMenu)
    {
        return FALSE;
    }
    g_clientMenu->Toggle();
    return TRUE;
}

BOOL IsZoomEnabled()
{
    return g_zoomModule && g_zoomModule->IsEnabled() ? TRUE : FALSE;
}

BOOL SetZoomEnabled(BOOL enabled)
{
    if (!g_zoomModule)
    {
        return FALSE;
    }
    g_zoomModule->SetEnabled(enabled != FALSE);
    return TRUE;
}

BOOL IsZoomActive()
{
    return g_zoomModule && g_zoomModule->IsZoomActive() ? TRUE : FALSE;
}

BOOL IsZoomKeybindEnabled()
{
    return g_zoomModule && g_zoomModule->IsKeybindEnabled() ? TRUE : FALSE;
}

BOOL SetZoomKeybindEnabled(BOOL enabled)
{
    if (!g_zoomModule)
    {
        return FALSE;
    }
    g_zoomModule->SetKeybindEnabled(enabled != FALSE);
    return TRUE;
}

BOOL IsNightVisionAvailable()
{
    return g_nightVisionModule && g_nightVisionModule->IsAvailable() ? TRUE : FALSE;
}

BOOL IsNightVisionEnabled()
{
    return g_nightVisionModule && g_nightVisionModule->IsEnabled() ? TRUE : FALSE;
}

BOOL SetNightVisionEnabled(BOOL enabled)
{
    if (!g_nightVisionModule || !g_nightVisionModule->IsAvailable())
    {
        return FALSE;
    }
    g_nightVisionModule->SetEnabled(enabled != FALSE);
    return TRUE;
}

BOOL IsNightVisionApplied()
{
    return g_nightVisionModule && g_nightVisionModule->IsApplied() ? TRUE : FALSE;
}

BOOL IsAutoArmorEnabled()
{
    return g_autoArmorModule && g_autoArmorModule->IsEnabled() ? TRUE : FALSE;
}

BOOL SetAutoArmorEnabled(BOOL enabled)
{
    if (!g_autoArmorModule)
    {
        return FALSE;
    }
    g_autoArmorModule->SetEnabled(enabled != FALSE);
    return TRUE;
}

BOOL IsTriggerbotEnabled()
{
    return g_triggerbotModule && g_triggerbotModule->IsEnabled() ? TRUE : FALSE;
}

BOOL SetTriggerbotEnabled(BOOL enabled)
{
    if (!g_triggerbotModule)
    {
        return FALSE;
    }
    g_triggerbotModule->SetEnabled(enabled != FALSE);
    return TRUE;
}

BOOL IsAutoClickerEnabled()
{
    return g_autoClickerModule && g_autoClickerModule->IsEnabled() ? TRUE : FALSE;
}

BOOL SetAutoClickerEnabled(BOOL enabled)
{
    if (!g_autoClickerModule)
    {
        return FALSE;
    }
    g_autoClickerModule->SetEnabled(enabled != FALSE);
    return TRUE;
}

BOOL IsAutoBreakEnabled()
{
    return g_autoBreakModule && g_autoBreakModule->IsEnabled() ? TRUE : FALSE;
}

BOOL SetAutoBreakEnabled(BOOL enabled)
{
    if (!g_autoBreakModule)
    {
        return FALSE;
    }
    g_autoBreakModule->SetEnabled(enabled != FALSE);
    return TRUE;
}
