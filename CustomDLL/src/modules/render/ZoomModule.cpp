#include "modules/render/ZoomModule.h"

#include <magnification.h>

namespace pebble
{
    constexpr wchar_t ZoomModule::kWindowClass[];
    std::atomic<ZoomModule*> ZoomModule::activeInstance_{nullptr};

    bool ZoomModule::Start(HMODULE module)
    {
        module_ = module;
        gameWindow_ = FindGameWindow();
        magnifierInitialized_ = MagInitialize() != FALSE;
        if (!magnifierInitialized_)
        {
            OutputDebugStringA("PebbleCore: Windows magnification runtime unavailable\n");
            return false;
        }

        if (gameWindow_ != nullptr)
        {
            activeInstance_.store(this, std::memory_order_release);
            SetLastError(ERROR_SUCCESS);
            originalGameWindowProcedure_ = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(
                    gameWindow_,
                    GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(GameWindowProcedure)));
            inputHookInstalled_ = originalGameWindowProcedure_ != nullptr;
            if (!inputHookInstalled_)
            {
                activeInstance_.store(nullptr, std::memory_order_release);
                OutputDebugStringA("PebbleCore: failed to hook Minecraft window input\n");
                Stop();
                return false;
            }
        }

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.hInstance = module_;
        windowClass.lpfnWndProc = ZoomWindowProcedure;
        windowClass.lpszClassName = kWindowClass;
        if (RegisterClassExW(&windowClass) == 0 &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            Stop();
            return false;
        }

        hostWindow_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT |
                WS_EX_NOACTIVATE | WS_EX_LAYERED,
            kWindowClass,
            L"PebbleCore Zoom",
            WS_POPUP,
            0,
            0,
            1,
            1,
            nullptr,
            nullptr,
            module_,
            nullptr);
        if (hostWindow_ == nullptr)
        {
            Stop();
            return false;
        }
        SetLayeredWindowAttributes(hostWindow_, 0, 255, LWA_ALPHA);

        magnifierWindow_ = CreateWindowExW(
            WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            L"Magnifier",
            L"PebbleCore Zoom Surface",
            WS_CHILD | WS_VISIBLE,
            0,
            0,
            1,
            1,
            hostWindow_,
            nullptr,
            module_,
            nullptr);
        if (magnifierWindow_ == nullptr)
        {
            Stop();
            return false;
        }
        // The magnifier is display-only. Disabling the child keeps it out of
        // mouse activation while the layered host returns HTTRANSPARENT.
        EnableWindow(magnifierWindow_, FALSE);

        MAGTRANSFORM transform{};
        transform.v[0][0] = kZoomFactor;
        transform.v[1][1] = kZoomFactor;
        transform.v[2][2] = 1.0f;
        if (!MagSetWindowTransform(magnifierWindow_, &transform))
        {
            Stop();
            return false;
        }

        HWND excluded[] = {hostWindow_};
        MagSetWindowFilterList(
            magnifierWindow_,
            MW_FILTERMODE_EXCLUDE,
            1,
            excluded);
        ShowWindow(hostWindow_, SW_HIDE);
        return true;
    }

    void ZoomModule::Tick()
    {
        if (zoomActive_.load(std::memory_order_acquire) &&
            GetForegroundWindow() != gameWindow_)
        {
            SetZoomActive(false);
        }

        if (zoomActive_.load(std::memory_order_acquire))
        {
            UpdateZoomWindow();
        }
    }

    void ZoomModule::Stop()
    {
        SetZoomActive(false);
        if (inputHookInstalled_ && gameWindow_ != nullptr && IsWindow(gameWindow_))
        {
            const auto currentProcedure = reinterpret_cast<WNDPROC>(
                GetWindowLongPtrW(gameWindow_, GWLP_WNDPROC));
            if (currentProcedure == GameWindowProcedure)
            {
                SetWindowLongPtrW(
                    gameWindow_,
                    GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(originalGameWindowProcedure_));
            }
        }
        activeInstance_.store(nullptr, std::memory_order_release);
        inputHookInstalled_ = false;
        originalGameWindowProcedure_ = nullptr;
        if (magnifierWindow_ != nullptr)
        {
            DestroyWindow(magnifierWindow_);
            magnifierWindow_ = nullptr;
        }
        if (hostWindow_ != nullptr)
        {
            DestroyWindow(hostWindow_);
            hostWindow_ = nullptr;
        }
        if (module_ != nullptr)
        {
            UnregisterClassW(kWindowClass, module_);
        }
        if (magnifierInitialized_)
        {
            MagUninitialize();
            magnifierInitialized_ = false;
        }
    }

    void ZoomModule::SetEnabled(bool enabled)
    {
        enabled_.store(enabled, std::memory_order_release);
        if (!enabled)
        {
            SetZoomActive(false);
        }
        else if (!keybindEnabled_.load(std::memory_order_acquire))
        {
            SetZoomActive(true);
        }
    }

    bool ZoomModule::IsEnabled() const
    {
        return enabled_.load(std::memory_order_acquire);
    }

    void ZoomModule::SetKeybindEnabled(bool enabled)
    {
        keybindEnabled_.store(enabled, std::memory_order_release);
        if (IsEnabled())
        {
            SetZoomActive(!enabled);
        }
    }

    bool ZoomModule::IsKeybindEnabled() const
    {
        return keybindEnabled_.load(std::memory_order_acquire);
    }

    void ZoomModule::SetZoomActive(bool active)
    {
        active = active &&
            IsEnabled() &&
            hostWindow_ != nullptr &&
            gameWindow_ != nullptr &&
            GetForegroundWindow() == gameWindow_;
        zoomActive_.store(active, std::memory_order_release);
        if (hostWindow_ != nullptr)
        {
            ShowWindow(hostWindow_, active ? SW_SHOWNOACTIVATE : SW_HIDE);
        }
    }

    bool ZoomModule::IsZoomActive() const
    {
        return zoomActive_.load(std::memory_order_acquire);
    }

    LRESULT CALLBACK ZoomModule::GameWindowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        ZoomModule* module = activeInstance_.load(std::memory_order_acquire);
        const WNDPROC original = module != nullptr
            ? module->originalGameWindowProcedure_
            : nullptr;
        const bool isZoomKey = wParam == 'C' &&
            (message == WM_KEYDOWN || message == WM_KEYUP ||
             message == WM_SYSKEYDOWN || message == WM_SYSKEYUP);
        if (module != nullptr && isZoomKey &&
            module->IsEnabled() && module->IsKeybindEnabled())
        {
            const bool isInitialPress =
                (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) &&
                (lParam & (1LL << 30)) == 0;
            if (isInitialPress)
            {
                module->HandleZoomKey();
            }
            return 0;
        }

        if (message == WM_NCDESTROY)
        {
            if (module != nullptr)
            {
                module->inputHookInstalled_ = false;
                module->gameWindow_ = nullptr;
                module->SetZoomActive(false);
            }
        }
        return original != nullptr
            ? CallWindowProcW(original, window, message, wParam, lParam)
            : DefWindowProcW(window, message, wParam, lParam);
    }

    LRESULT CALLBACK ZoomModule::ZoomWindowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        switch (message)
        {
            case WM_NCHITTEST:
                return HTTRANSPARENT;
            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;
            case WM_ACTIVATE:
                return 0;
            default:
                return DefWindowProcW(window, message, wParam, lParam);
        }
    }

    void ZoomModule::HandleZoomKey()
    {
        if (GetForegroundWindow() != gameWindow_)
        {
            return;
        }
        SetZoomActive(!IsZoomActive());
    }

    HWND ZoomModule::FindGameWindow()
    {
        HWND result = nullptr;
        EnumWindows(FindGameWindowCallback, reinterpret_cast<LPARAM>(&result));
        return result;
    }

    BOOL CALLBACK ZoomModule::FindGameWindowCallback(HWND window, LPARAM parameter)
    {
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId != GetCurrentProcessId() || !IsWindowVisible(window))
        {
            return TRUE;
        }

        RECT bounds{};
        if (!GetWindowRect(window, &bounds) ||
            bounds.right - bounds.left < 400 ||
            bounds.bottom - bounds.top < 300)
        {
            return TRUE;
        }

        *reinterpret_cast<HWND*>(parameter) = window;
        return FALSE;
    }

    void ZoomModule::UpdateZoomWindow()
    {
        if (gameWindow_ == nullptr || !IsWindow(gameWindow_))
        {
            gameWindow_ = FindGameWindow();
        }
        if (gameWindow_ == nullptr)
        {
            return;
        }

        RECT gameBounds{};
        if (!GetWindowRect(gameWindow_, &gameBounds))
        {
            return;
        }

        const int width = gameBounds.right - gameBounds.left;
        const int height = gameBounds.bottom - gameBounds.top;
        if (width <= 0 || height <= 0)
        {
            return;
        }

        SetWindowPos(
            hostWindow_,
            HWND_TOPMOST,
            gameBounds.left,
            gameBounds.top,
            width,
            height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SetWindowPos(
            magnifierWindow_,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOACTIVATE | SWP_NOZORDER);

        const int sourceWidth = static_cast<int>(width / kZoomFactor);
        const int sourceHeight = static_cast<int>(height / kZoomFactor);
        const int centerX = gameBounds.left + width / 2;
        const int centerY = gameBounds.top + height / 2;
        RECT source{
            centerX - sourceWidth / 2,
            centerY - sourceHeight / 2,
            centerX + sourceWidth / 2,
            centerY + sourceHeight / 2
        };
        MagSetWindowSource(magnifierWindow_, source);
        InvalidateRect(magnifierWindow_, nullptr, TRUE);
    }
}
