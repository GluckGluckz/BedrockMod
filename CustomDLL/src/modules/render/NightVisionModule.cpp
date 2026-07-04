#include "modules/render/NightVisionModule.h"

#include "core/SignatureScanner.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <windows.h>
#include <MinHook.h>

namespace
{
    constexpr int kNightVisionEffectId = 16;
    constexpr int kInfiniteDuration = 20'000'000;
    constexpr std::string_view kLocalPlayerChangedPattern =
        "56 57 48 83 EC 28 48 89 CE 48 8B 89 80 00 00 00 "
        "48 85 C9 74 ? 48 8B 01 48 8B 40 10 48 8B 3A "
        "FF 15 ? ? ? ? 48 85 FF 74 ? 48 8B 4E 40";

    std::mutex g_logMutex;

    // Minecraft.Windows.exe runs in a UWP AppContainer. GetTempPath resolves
    // to the sandbox's own writable temp folder, so we mirror every diagnostic
    // line both to OutputDebugStringA (DebugView) and to a file the user can
    // simply open and paste back. A console (AllocConsole) is unreliable for a
    // UWP GUI process, so we don't use one.
    const std::string& LogFilePath()
    {
        static const std::string path = [] {
            char temp[MAX_PATH]{};
            const DWORD length = GetTempPathA(MAX_PATH, temp);
            std::string base = length != 0 ? std::string(temp, length) : std::string();
            return base + "PebbleCore.log";
        }();
        return path;
    }

    void NvWriteLine(const char* text)
    {
        std::scoped_lock lock(g_logMutex);
        OutputDebugStringA("PebbleCore[NV]: ");
        OutputDebugStringA(text);
        OutputDebugStringA("\n");
        if (FILE* file = nullptr; fopen_s(&file, LogFilePath().c_str(), "a") == 0 && file)
        {
            SYSTEMTIME now{};
            GetLocalTime(&now);
            fprintf(file, "[%02u:%02u:%02u.%03u] %s\n",
                now.wHour, now.wMinute, now.wSecond, now.wMilliseconds, text);
            fclose(file);
        }
    }

    void NvLog(const char* format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
        va_end(args);
        NvWriteLine(buffer);
    }

    // Dump `count` bytes at `address` as offset/hex/ascii rows so we can locate
    // the embedded EnTT registry pointer and entity id inside the player object.
    void NvHexDump(const char* label, const void* address, std::size_t count)
    {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(address);
        NvLog("%s base=%p (%zu bytes):", label, address, count);
        for (std::size_t row = 0; row < count; row += 16)
        {
            char line[128];
            int written = _snprintf_s(line, sizeof(line), _TRUNCATE, "  +0x%02zX: ", row);
            for (std::size_t col = 0; col < 16 && row + col < count; ++col)
            {
                written += _snprintf_s(line + written, sizeof(line) - written, _TRUNCATE,
                    "%02X ", bytes[row + col]);
            }
            NvWriteLine(line);
        }
    }
}

namespace pebble
{
    std::atomic<NightVisionModule*> NightVisionModule::activeInstance_{nullptr};
    NightVisionModule::LocalPlayerChangedFunction
        NightVisionModule::originalLocalPlayerChanged_ = nullptr;

    NightVisionModule::NightVisionModule(GameBuild build)
        : build_(build)
    {
    }

    bool NightVisionModule::Start()
    {
        if (build_.major != 1 || build_.minor != 26 || build_.patch != 3202)
        {
            OutputDebugStringA("PebbleCore: Night Vision disabled for an unsupported game build\n");
            return false;
        }

        const auto callbackMatches = SignatureScanner::FindAll(kLocalPlayerChangedPattern);
        if (callbackMatches.size() != 1)
        {
            OutputDebugStringA(
                "PebbleCore: Night Vision local-player callback was not unique\n");
            return false;
        }

        const MH_STATUS initializeStatus = MH_Initialize();
        if (initializeStatus != MH_OK && initializeStatus != MH_ERROR_ALREADY_INITIALIZED)
        {
            return false;
        }

        hookTarget_ = reinterpret_cast<void*>(callbackMatches[0]);
        activeInstance_.store(this, std::memory_order_release);
        if (MH_CreateHook(
                hookTarget_,
                reinterpret_cast<void*>(LocalPlayerChangedDetour),
                reinterpret_cast<void**>(&originalLocalPlayerChanged_)) != MH_OK ||
            MH_EnableHook(hookTarget_) != MH_OK)
        {
            activeInstance_.store(nullptr, std::memory_order_release);
            if (hookTarget_ != nullptr)
            {
                MH_RemoveHook(hookTarget_);
            }
            MH_Uninitialize();
            hookTarget_ = nullptr;
            originalLocalPlayerChanged_ = nullptr;
            return false;
        }

        hookInstalled_ = true;
        available_.store(true, std::memory_order_release);
        status_.store(NightVisionStatus::Idle, std::memory_order_release);
        NvLog("Start: hook installed at rva-target %p, module available", hookTarget_);
        return true;
    }

    void NightVisionModule::Stop()
    {
        if (!hookInstalled_)
        {
            return;
        }
        enabled_.store(false, std::memory_order_release);
        stopRequested_.store(true, std::memory_order_release);
        ApplyCurrentState();
        MH_DisableHook(hookTarget_);
        MH_RemoveHook(hookTarget_);
        MH_Uninitialize();
        activeInstance_.store(nullptr, std::memory_order_release);
        originalLocalPlayerChanged_ = nullptr;
        localPlayer_.store(nullptr, std::memory_order_release);
        hookTarget_ = nullptr;
        hookInstalled_ = false;
        available_.store(false, std::memory_order_release);
        status_.store(NightVisionStatus::Unavailable, std::memory_order_release);
    }

    void NightVisionModule::SetEnabled(bool enabled)
    {
        enabled_.store(enabled && IsAvailable(), std::memory_order_release);
        NvLog("SetEnabled(%d) -> enabled=%d available=%d player=%p",
            enabled ? 1 : 0,
            enabled_.load(std::memory_order_acquire) ? 1 : 0,
            IsAvailable() ? 1 : 0,
            localPlayer_.load(std::memory_order_acquire));
        ApplyCurrentState();
    }

    bool NightVisionModule::IsEnabled() const
    {
        return enabled_.load(std::memory_order_acquire);
    }

    bool NightVisionModule::IsAvailable() const
    {
        return available_.load(std::memory_order_acquire);
    }

    bool NightVisionModule::IsApplied() const
    {
        return applied_.load(std::memory_order_acquire);
    }

    NightVisionStatus NightVisionModule::Status() const
    {
        return status_.load(std::memory_order_acquire);
    }

    void __fastcall NightVisionModule::LocalPlayerChangedDetour(
        void* callback,
        const void** playerArgument)
    {
        NightVisionModule* module = activeInstance_.load(std::memory_order_acquire);
        void* player = playerArgument == nullptr
            ? nullptr
            : const_cast<void*>(*playerArgument);

        NvLog("Detour fired: callback=%p playerArg=%p *playerArg=%p",
            callback, static_cast<const void*>(playerArgument), player);

        if (module != nullptr && player == nullptr)
        {
            module->OnLocalPlayerChanged(nullptr);
        }

        if (originalLocalPlayerChanged_ != nullptr)
        {
            originalLocalPlayerChanged_(callback, playerArgument);
        }

        if (module != nullptr && player != nullptr)
        {
            module->OnLocalPlayerChanged(player);
        }
    }

    void NightVisionModule::OnLocalPlayerChanged(void* player)
    {
        if (player == nullptr)
        {
            ApplyCurrentState();
            localPlayer_.store(nullptr, std::memory_order_release);
            return;
        }

        localPlayer_.store(player, std::memory_order_release);
        ApplyCurrentState();
    }

    MobEffectsComponent* NightVisionModule::GetEffectsComponent(void* player) const
    {
        if (player == nullptr)
        {
            return nullptr;
        }

        auto* registry = *reinterpret_cast<entt::basic_registry<EntityId>**>(
            reinterpret_cast<std::uint8_t*>(player) + 0x10);
        const std::uint32_t entityId = *reinterpret_cast<const std::uint32_t*>(
            reinterpret_cast<const std::uint8_t*>(player) + 0x18);
        NvLog("GetEffectsComponent: player=%p registry@0x10=%p entityId@0x18=0x%X",
            player, static_cast<void*>(registry), entityId);
        // Dump the head of the player object so we can verify where the EnTT
        // registry pointer and entity id actually live for this build.
        NvHexDump("player object", player, 0x40);
        if (registry == nullptr)
        {
            return nullptr;
        }
        MobEffectsComponent* component =
            registry->try_get<MobEffectsComponent>(EntityId(entityId));
        NvLog("GetEffectsComponent: try_get<MobEffectsComponent> -> component=%p",
            static_cast<void*>(component));

        if (component == nullptr)
        {
            // Decisive diagnostic: is our component's EnTT type-hash even present
            // among the registry's pools? If not, the hash/type name differs from
            // Minecraft's; if yes, the entity id or lookup path is the problem.
            const auto ourHash = entt::type_hash<MobEffectsComponent>::value();
            int poolCount = 0;
            bool hashPresent = false;
            for (auto&& [id, cpool] : registry->storage())
            {
                ++poolCount;
                if (id == ourHash)
                {
                    hashPresent = true;
                }
            }
            NvLog("diag: ourTypeHash=0x%X pools=%d hashPresent=%d entityValid=%d",
                static_cast<unsigned>(ourHash), poolCount, hashPresent ? 1 : 0,
                registry->valid(EntityId(entityId)) ? 1 : 0);
        }
        return component;
    }

    void NightVisionModule::ApplyCurrentState()
    {
        std::scoped_lock lock(effectMutex_);
        const bool enabled = enabled_.load(std::memory_order_acquire) &&
            !stopRequested_.load(std::memory_order_acquire);
        void* player = localPlayer_.load(std::memory_order_acquire);

        if (player == nullptr)
        {
            status_.store(
                enabled ? NightVisionStatus::WaitingForPlayer : NightVisionStatus::Idle,
                std::memory_order_release);
            NvLog("ApplyCurrentState: no local player (enabled=%d) -> %s",
                enabled ? 1 : 0,
                enabled ? "WaitingForPlayer" : "Idle");
            return;
        }

        MobEffectsComponent* component = GetEffectsComponent(player);
        if (component == nullptr)
        {
            applied_.store(false, std::memory_order_release);
            status_.store(NightVisionStatus::NoComponent, std::memory_order_release);
            NvLog("ApplyCurrentState: component not resolved -> NoComponent");
            return;
        }

        NvLog("ApplyCurrentState: component=%p effects=%zu enabled=%d",
            static_cast<void*>(component), component->effects.size(), enabled ? 1 : 0);

        if (enabled)
        {
            Apply(*component);
            status_.store(NightVisionStatus::Applied, std::memory_order_release);
            NvLog("ApplyCurrentState: applied effect %d, effects now=%zu",
                kNightVisionEffectId, component->effects.size());
        }
        else
        {
            if (applied_.load(std::memory_order_acquire))
            {
                Remove(*component);
            }
            status_.store(NightVisionStatus::Idle, std::memory_order_release);
        }
    }

    void NightVisionModule::Apply(MobEffectsComponent& component)
    {
        auto effect = std::find_if(
            component.effects.begin(),
            component.effects.end(),
            [](const MobEffectInstance& item) { return item.id == kNightVisionEffectId; });

        if (!applied_.load(std::memory_order_acquire))
        {
            insertedEffect_ = effect == component.effects.end();
            hadExistingEffect_ = !insertedEffect_;
            if (hadExistingEffect_)
            {
                existingEffect_ = {
                    effect->duration,
                    effect->unknown,
                    effect->durationEasy,
                    effect->durationNormal,
                    effect->durationHard,
                    effect->amplifier,
                    effect->displayOnScreenTextureAnimation,
                    effect->ambient,
                    effect->noCounter,
                    effect->effectVisible
                };
            }
        }

        if (effect == component.effects.end())
        {
            if (applied_.load(std::memory_order_acquire))
            {
                hadExistingEffect_ = false;
            }
            MobEffectInstance instance{};
            instance.id = kNightVisionEffectId;
            component.effects.push_back(instance);
            effect = std::prev(component.effects.end());
            insertedEffect_ = true;
        }

        effect->duration = kInfiniteDuration;
        effect->durationEasy = kInfiniteDuration;
        effect->durationNormal = kInfiniteDuration;
        effect->durationHard = kInfiniteDuration;
        effect->amplifier = 0;
        effect->displayOnScreenTextureAnimation = true;
        effect->ambient = false;
        effect->noCounter = true;
        effect->effectVisible = true;
        applied_.store(true, std::memory_order_release);
    }

    void NightVisionModule::Remove(MobEffectsComponent& component)
    {
        const auto effect = std::find_if(
            component.effects.begin(),
            component.effects.end(),
            [](const MobEffectInstance& item) { return item.id == kNightVisionEffectId; });
        if (effect != component.effects.end())
        {
            if (hadExistingEffect_)
            {
                effect->duration = existingEffect_.duration;
                effect->unknown = existingEffect_.unknown;
                effect->durationEasy = existingEffect_.durationEasy;
                effect->durationNormal = existingEffect_.durationNormal;
                effect->durationHard = existingEffect_.durationHard;
                effect->amplifier = existingEffect_.amplifier;
                effect->displayOnScreenTextureAnimation = existingEffect_.displayAnimation;
                effect->ambient = existingEffect_.ambient;
                effect->noCounter = existingEffect_.noCounter;
                effect->effectVisible = existingEffect_.visible;
            }
            else if (insertedEffect_)
            {
                component.effects.erase(effect);
            }
        }

        insertedEffect_ = false;
        hadExistingEffect_ = false;
        applied_.store(false, std::memory_order_release);
    }
}
