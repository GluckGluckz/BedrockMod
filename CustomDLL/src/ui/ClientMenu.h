#pragma once

#include "cosmetics/CosmeticRegistry.h"
#include "modules/render/ZoomModule.h"
#include "modules/render/NightVisionModule.h"
#include "modules/combat/AutoArmorModule.h"
#include "modules/combat/TriggerbotModule.h"
#include "modules/player/AutoClickerModule.h"
#include "modules/player/AutoBreakModule.h"

#include <array>
#include <atomic>
#include <windows.h>

namespace pebble
{
    class ClientMenu
    {
    public:
        ClientMenu(
            CosmeticRegistry& cosmetics,
            ZoomModule& zoom,
            NightVisionModule& nightVision,
            AutoArmorModule& autoArmor,
            TriggerbotModule& triggerbot,
            AutoClickerModule& autoClicker,
            AutoBreakModule& autoBreak);

        bool Start(HMODULE module);
        void Tick();
        void Stop();
        bool IsVisible() const;
        void Toggle();

    private:
        struct MenuCosmetic
        {
            const char* id;
            const wchar_t* name;
            const wchar_t* description;
        };

        enum class Category
        {
            Cosmetics,
            Render,
            Combat,
            Player
        };

        static LRESULT CALLBACK WindowProcedure(
            HWND window,
            UINT message,
            WPARAM wParam,
            LPARAM lParam);
        static BOOL CALLBACK FindGameWindowCallback(HWND window, LPARAM parameter);

        LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
        void Paint();
        void HandleClick(int x, int y);
        void ApplyToggle();
        void UpdateBounds();
        HWND FindGameWindow() const;

        static constexpr wchar_t kWindowClass[] = L"PebbleCore.ClientMenu";
        static constexpr int kPreferredWidth = 760;
        static constexpr int kPreferredHeight = 500;

        CosmeticRegistry& cosmetics_;
        ZoomModule& zoom_;
        NightVisionModule& nightVision_;
        AutoArmorModule& autoArmor_;
        TriggerbotModule& triggerbot_;
        AutoClickerModule& autoClicker_;
        AutoBreakModule& autoBreak_;
        HMODULE module_ = nullptr;
        HWND window_ = nullptr;
        HWND gameWindow_ = nullptr;
        std::atomic<bool> visible_{false};
        Category activeCategory_ = Category::Cosmetics;
        std::array<MenuCosmetic, 3> menuCosmetics_{{
            {"pebblecore:founder_cape", L"Founder Cape", L"A signature PebbleCore cape."},
            {"pebblecore:pebble_pack", L"Pebble Pack", L"A compact adventure backpack."},
            {"pebblecore:quest_crown", L"Quest Crown", L"A crown for dedicated questers."}
        }};
    };
}
