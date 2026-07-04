#pragma once

#include <cstdint>
#include <windows.h>

#ifdef PEBBLECORE_EXPORTS
#define PEBBLECORE_API extern "C" __declspec(dllexport)
#else
#define PEBBLECORE_API extern "C" __declspec(dllimport)
#endif

// Stable C ABI exposed to the injector and future tooling.
PEBBLECORE_API BOOL IsModActive();
PEBBLECORE_API const char* GetModName();
PEBBLECORE_API std::uint32_t GetModApiVersion();
PEBBLECORE_API BOOL RequestShutdown();
PEBBLECORE_API const char* GetDetectedGameVersion();
PEBBLECORE_API std::uint32_t GetRenderHookStatus();
PEBBLECORE_API std::uint32_t GetCosmeticCount();
PEBBLECORE_API BOOL SetCosmeticEnabled(const char* cosmeticId, BOOL enabled);
PEBBLECORE_API BOOL IsCosmeticEnabled(const char* cosmeticId);
PEBBLECORE_API BOOL IsMenuVisible();
PEBBLECORE_API BOOL ToggleMenu();
PEBBLECORE_API BOOL IsZoomEnabled();
PEBBLECORE_API BOOL SetZoomEnabled(BOOL enabled);
PEBBLECORE_API BOOL IsZoomActive();
PEBBLECORE_API BOOL IsZoomKeybindEnabled();
PEBBLECORE_API BOOL SetZoomKeybindEnabled(BOOL enabled);
PEBBLECORE_API BOOL IsNightVisionAvailable();
PEBBLECORE_API BOOL IsNightVisionEnabled();
PEBBLECORE_API BOOL SetNightVisionEnabled(BOOL enabled);
PEBBLECORE_API BOOL IsNightVisionApplied();
PEBBLECORE_API BOOL IsAutoArmorEnabled();
PEBBLECORE_API BOOL SetAutoArmorEnabled(BOOL enabled);
PEBBLECORE_API BOOL IsTriggerbotEnabled();
PEBBLECORE_API BOOL SetTriggerbotEnabled(BOOL enabled);
PEBBLECORE_API BOOL IsAutoClickerEnabled();
PEBBLECORE_API BOOL SetAutoClickerEnabled(BOOL enabled);
PEBBLECORE_API BOOL IsAutoBreakEnabled();
PEBBLECORE_API BOOL SetAutoBreakEnabled(BOOL enabled);
