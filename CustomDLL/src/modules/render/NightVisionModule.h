#pragma once

#include "core/GameBuild.h"
#include "game/MinecraftEffectTypes.h"

#include <atomic>
#include <cstdint>
#include <mutex>

namespace pebble
{
    enum class NightVisionStatus : std::uint32_t
    {
        Unavailable = 0,      // Start() never succeeded (signature/build gate)
        Idle,                 // available, module disabled
        WaitingForPlayer,     // enabled, but the local-player pointer is null
        NoComponent,          // player captured, but MobEffectsComponent not resolved
        Applied               // effect written into the live component
    };

    class NightVisionModule
    {
    public:
        explicit NightVisionModule(GameBuild build);

        bool Start();
        void Stop();
        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        bool IsAvailable() const;
        bool IsApplied() const;
        NightVisionStatus Status() const;

    private:
        using LocalPlayerChangedFunction = void(__fastcall*)(void*, const void**);

        struct EffectSnapshot
        {
            int duration = 0;
            float unknown = 0.0f;
            int durationEasy = 0;
            int durationNormal = 0;
            int durationHard = 0;
            int amplifier = 0;
            bool displayAnimation = false;
            bool ambient = false;
            bool noCounter = false;
            bool visible = false;
        };

        static void __fastcall LocalPlayerChangedDetour(
            void* callback,
            const void** playerArgument);
        void OnLocalPlayerChanged(void* player);
        MobEffectsComponent* GetEffectsComponent(void* player) const;
        void Apply(MobEffectsComponent& component);
        void Remove(MobEffectsComponent& component);
        void ApplyCurrentState();

        static std::atomic<NightVisionModule*> activeInstance_;
        static LocalPlayerChangedFunction originalLocalPlayerChanged_;

        GameBuild build_;
        std::atomic<bool> enabled_{false};
        std::atomic<bool> available_{false};
        std::atomic<bool> applied_{false};
        std::atomic<bool> stopRequested_{false};
        std::atomic<NightVisionStatus> status_{NightVisionStatus::Unavailable};
        std::atomic<void*> localPlayer_{nullptr};
        mutable std::mutex effectMutex_;
        void* hookTarget_ = nullptr;
        bool hookInstalled_ = false;
        bool insertedEffect_ = false;
        bool hadExistingEffect_ = false;
        EffectSnapshot existingEffect_{};
    };
}
