#pragma once

#include <atomic>
#include <windows.h>

namespace pebble
{
    class ZoomModule
    {
    public:
        bool Start(HMODULE module);
        void Tick();
        void Stop();

        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        void SetKeybindEnabled(bool enabled);
        bool IsKeybindEnabled() const;
        void SetZoomActive(bool active);
        bool IsZoomActive() const;

    private:
        static LRESULT CALLBACK GameWindowProcedure(
            HWND window,
            UINT message,
            WPARAM wParam,
            LPARAM lParam);
        static LRESULT CALLBACK ZoomWindowProcedure(
            HWND window,
            UINT message,
            WPARAM wParam,
            LPARAM lParam);
        static HWND FindGameWindow();
        static BOOL CALLBACK FindGameWindowCallback(HWND window, LPARAM parameter);
        void HandleZoomKey();
        void UpdateZoomWindow();

        static constexpr wchar_t kWindowClass[] = L"PebbleCore.ZoomHost";
        static constexpr float kZoomFactor = 2.0f;
        static std::atomic<ZoomModule*> activeInstance_;

        HMODULE module_ = nullptr;
        HWND gameWindow_ = nullptr;
        HWND hostWindow_ = nullptr;
        HWND magnifierWindow_ = nullptr;
        WNDPROC originalGameWindowProcedure_ = nullptr;
        bool magnifierInitialized_ = false;
        bool inputHookInstalled_ = false;
        std::atomic<bool> enabled_{false};
        std::atomic<bool> keybindEnabled_{true};
        std::atomic<bool> zoomActive_{false};
    };
}
