#include "ui/ClientMenu.h"

#include <algorithm>
#include <windowsx.h>

namespace
{
    constexpr COLORREF kBackground = RGB(14, 17, 22);
    constexpr COLORREF kPanel = RGB(22, 26, 33);
    constexpr COLORREF kCard = RGB(30, 35, 44);
    constexpr COLORREF kCardHover = RGB(36, 42, 52);
    constexpr COLORREF kAccent = RGB(82, 178, 111);
    constexpr COLORREF kMuted = RGB(147, 157, 173);
    constexpr COLORREF kWhite = RGB(240, 244, 248);

    void Fill(HDC device, const RECT& rectangle, COLORREF color)
    {
        const HBRUSH brush = CreateSolidBrush(color);
        FillRect(device, &rectangle, brush);
        DeleteObject(brush);
    }

    void FillRounded(HDC device, const RECT& rectangle, int radius, COLORREF color)
    {
        const HBRUSH brush = CreateSolidBrush(color);
        const HGDIOBJ oldBrush = SelectObject(device, brush);
        const HGDIOBJ oldPen = SelectObject(device, GetStockObject(NULL_PEN));
        RoundRect(
            device,
            rectangle.left,
            rectangle.top,
            rectangle.right,
            rectangle.bottom,
            radius,
            radius);
        SelectObject(device, oldPen);
        SelectObject(device, oldBrush);
        DeleteObject(brush);
    }

    void DrawLabel(
        HDC device,
        const wchar_t* text,
        RECT rectangle,
        COLORREF color,
        HFONT font,
        UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE)
    {
        SetTextColor(device, color);
        SetBkMode(device, TRANSPARENT);
        const HGDIOBJ oldFont = SelectObject(device, font);
        DrawTextW(device, text, -1, &rectangle, format);
        SelectObject(device, oldFont);
    }

    // Standard 82px-tall module card: title, colored subtitle, and a toggle
    // switch on the right. Cards stack every 92px from `top`.
    struct CardFonts
    {
        HFONT title;
        HFONT small;
    };

    void DrawToggleCard(
        HDC device,
        const RECT& client,
        int top,
        const wchar_t* title,
        const wchar_t* subtitle,
        COLORREF subtitleColor,
        bool enabled,
        const CardFonts& fonts)
    {
        const RECT card{216, top, client.right - 32, top + 82};
        FillRounded(device, card, 12, kCard);
        DrawLabel(device, title, {238, top + 10, client.right - 150, top + 40}, kWhite, fonts.title);
        DrawLabel(device, subtitle, {238, top + 40, client.right - 150, top + 68}, subtitleColor, fonts.small);

        const RECT track{client.right - 112, top + 25, client.right - 52, top + 57};
        FillRounded(device, track, 18, enabled ? kAccent : RGB(63, 70, 82));
        const int knob = enabled ? track.right - 28 : track.left + 4;
        FillRounded(device, {knob, track.top + 4, knob + 24, track.bottom - 4}, 14, kWhite);
    }

    // Vertical hit region for the Nth stacked card (matches DrawToggleCard).
    bool HitCard(const RECT& client, int x, int y, int top)
    {
        return x >= 216 && x <= client.right - 32 && y >= top && y <= top + 82;
    }
}

namespace pebble
{
    constexpr wchar_t ClientMenu::kWindowClass[];

    ClientMenu::ClientMenu(
        CosmeticRegistry& cosmetics,
        ZoomModule& zoom,
        NightVisionModule& nightVision,
        AutoArmorModule& autoArmor,
        TriggerbotModule& triggerbot,
        AutoClickerModule& autoClicker,
        AutoBreakModule& autoBreak)
        : cosmetics_(cosmetics),
          zoom_(zoom),
          nightVision_(nightVision),
          autoArmor_(autoArmor),
          triggerbot_(triggerbot),
          autoClicker_(autoClicker),
          autoBreak_(autoBreak)
    {
    }

    bool ClientMenu::Start(HMODULE module)
    {
        module_ = module;
        gameWindow_ = FindGameWindow();

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.hInstance = module_;
        windowClass.lpfnWndProc = WindowProcedure;
        windowClass.lpszClassName = kWindowClass;
        windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        windowClass.hbrBackground = nullptr;
        windowClass.style = CS_HREDRAW | CS_VREDRAW;

        if (RegisterClassExW(&windowClass) == 0 &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return false;
        }

        window_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            kWindowClass,
            L"PebbleCore",
            WS_POPUP,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            kPreferredWidth,
            kPreferredHeight,
            nullptr,
            nullptr,
            module_,
            this);
        return window_ != nullptr;
    }

    void ClientMenu::Tick()
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        const HWND foreground = GetForegroundWindow();
        const bool menuVisible = visible_.load(std::memory_order_acquire);
        const bool ownsForeground = foreground == gameWindow_ || foreground == window_;
        if (menuVisible && !ownsForeground)
        {
            Toggle();
            return;
        }
        if (ownsForeground && (GetAsyncKeyState(VK_RSHIFT) & 1) != 0)
        {
            Toggle();
        }
        if (menuVisible) UpdateBounds();
    }

    void ClientMenu::Stop()
    {
        if (window_ != nullptr)
        {
            DestroyWindow(window_);
            window_ = nullptr;
        }
        if (module_ != nullptr)
        {
            UnregisterClassW(kWindowClass, module_);
        }
        visible_.store(false, std::memory_order_release);
    }

    bool ClientMenu::IsVisible() const
    {
        return visible_.load(std::memory_order_acquire);
    }

    void ClientMenu::Toggle()
    {
        if (window_ == nullptr)
        {
            return;
        }
        PostMessageW(window_, WM_APP + 1, 0, 0);
    }

    void ClientMenu::ApplyToggle()
    {
        const bool show = !visible_.load(std::memory_order_acquire);
        visible_.store(show, std::memory_order_release);
        if (show)
        {
            UpdateBounds();
            ShowWindow(window_, SW_SHOW);
            SetForegroundWindow(window_);
            InvalidateRect(window_, nullptr, FALSE);
        }
        else
        {
            ShowWindow(window_, SW_HIDE);
            if (gameWindow_ != nullptr)
            {
                SetForegroundWindow(gameWindow_);
            }
        }
    }

    LRESULT CALLBACK ClientMenu::WindowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        ClientMenu* menu = reinterpret_cast<ClientMenu*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            menu = static_cast<ClientMenu*>(create->lpCreateParams);
            menu->window_ = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(menu));
        }

        return menu != nullptr
            ? menu->HandleMessage(message, wParam, lParam)
            : DefWindowProcW(window, message, wParam, lParam);
    }

    BOOL CALLBACK ClientMenu::FindGameWindowCallback(HWND window, LPARAM parameter)
    {
        auto* result = reinterpret_cast<HWND*>(parameter);
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

        *result = window;
        return FALSE;
    }

    LRESULT ClientMenu::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
            case WM_PAINT:
                Paint();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_LBUTTONUP:
                HandleClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE)
                {
                    Toggle();
                    return 0;
                }
                break;
            case WM_CLOSE:
                Toggle();
                return 0;
            case WM_APP + 1:
                ApplyToggle();
                return 0;
            default:
                break;
        }

        return DefWindowProcW(window_, message, wParam, lParam);
    }

    void ClientMenu::Paint()
    {
        PAINTSTRUCT paint{};
        const HDC device = BeginPaint(window_, &paint);
        RECT client{};
        GetClientRect(window_, &client);

        Fill(device, client, kBackground);
        RECT sidebar{0, 0, 184, client.bottom};
        Fill(device, sidebar, kPanel);
        RECT accent{0, 0, 5, client.bottom};
        Fill(device, accent, kAccent);

        const HFONT logoFont = CreateFontW(
            25, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Segoe UI");
        const HFONT titleFont = CreateFontW(
            20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Segoe UI");
        const HFONT bodyFont = CreateFontW(
            15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Segoe UI");
        const HFONT smallFont = CreateFontW(
            13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH, L"Segoe UI");

        DrawLabel(device, L"PEBBLE", {24, 20, 160, 52}, kWhite, logoFont);
        DrawLabel(device, L"CORE", {24, 48, 160, 75}, kAccent, titleFont);
        FillRounded(
            device,
            {16, 112, 168, 158},
            10,
            activeCategory_ == Category::Cosmetics ? kCardHover : kCard);
        DrawLabel(device, L"Cosmetics", {34, 112, 162, 158}, kWhite, bodyFont);
        FillRounded(
            device,
            {16, 166, 168, 212},
            10,
            activeCategory_ == Category::Render ? kCardHover : kCard);
        DrawLabel(device, L"Render", {34, 166, 162, 212}, kWhite, bodyFont);
        FillRounded(
            device,
            {16, 220, 168, 266},
            10,
            activeCategory_ == Category::Combat ? kCardHover : kCard);
        DrawLabel(device, L"Combat", {34, 220, 162, 266}, kWhite, bodyFont);
        FillRounded(
            device,
            {16, 274, 168, 320},
            10,
            activeCategory_ == Category::Player ? kCardHover : kCard);
        DrawLabel(device, L"Player", {34, 274, 162, 320}, kWhite, bodyFont);
        DrawLabel(
            device,
            L"RIGHT SHIFT  •  CLOSE",
            {20, client.bottom - 48, 172, client.bottom - 18},
            kMuted,
            smallFont);

        const wchar_t* headerTitle = L"Cosmetics";
        const wchar_t* headerSubtitle = L"Local visual customizations for PebbleCore";
        switch (activeCategory_)
        {
        case Category::Render:
            headerTitle = L"Render";
            headerSubtitle = L"Client-side camera and visual modules";
            break;
        case Category::Combat:
            headerTitle = L"Combat";
            headerSubtitle = L"Combat automation modules";
            break;
        case Category::Player:
            headerTitle = L"Player";
            headerSubtitle = L"Player input automation modules";
            break;
        case Category::Cosmetics:
        default:
            break;
        }
        DrawLabel(device, headerTitle, {220, 24, 520, 60}, kWhite, logoFont);
        DrawLabel(
            device,
            headerSubtitle,
            {220, 58, client.right - 60, 84},
            kMuted,
            bodyFont);
        DrawLabel(
            device,
            L"×",
            {client.right - 52, 18, client.right - 18, 48},
            kMuted,
            titleFont,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (activeCategory_ == Category::Cosmetics)
        {
            for (std::size_t index = 0; index < menuCosmetics_.size(); ++index)
            {
                const int top = 106 + static_cast<int>(index) * 104;
                const RECT card{216, top, client.right - 32, top + 82};
                FillRounded(device, card, 12, index == 0 ? kCardHover : kCard);

                DrawLabel(
                    device,
                    menuCosmetics_[index].name,
                    {238, top + 10, client.right - 150, top + 40},
                    kWhite,
                    titleFont);
                DrawLabel(
                    device,
                    menuCosmetics_[index].description,
                    {238, top + 40, client.right - 150, top + 68},
                    kMuted,
                    smallFont);

                const bool enabled = cosmetics_.IsEnabled(menuCosmetics_[index].id);
                const RECT switchTrack{
                    client.right - 112,
                    top + 25,
                    client.right - 52,
                    top + 57
                };
                FillRounded(device, switchTrack, 18, enabled ? kAccent : RGB(63, 70, 82));
                const int knobLeft = enabled ? switchTrack.right - 28 : switchTrack.left + 4;
                FillRounded(
                    device,
                    {knobLeft, switchTrack.top + 4, knobLeft + 24, switchTrack.bottom - 4},
                    14,
                    kWhite);
            }
        }
        else if (activeCategory_ == Category::Render)
        {
            const int top = 106;
            const RECT card{216, top, client.right - 32, top + 154};
            FillRounded(device, card, 12, kCardHover);
            DrawLabel(device, L"Zoom", {238, top + 12, client.right - 150, top + 42}, kWhite, titleFont);
            DrawLabel(
                device,
                L"Magnifies the center of the game view.",
                {238, top + 42, client.right - 150, top + 68},
                kMuted,
                smallFont);

            const bool moduleEnabled = zoom_.IsEnabled();
            const RECT moduleSwitch{client.right - 112, top + 20, client.right - 52, top + 52};
            FillRounded(device, moduleSwitch, 18, moduleEnabled ? kAccent : RGB(63, 70, 82));
            const int moduleKnob = moduleEnabled
                ? moduleSwitch.right - 28
                : moduleSwitch.left + 4;
            FillRounded(
                device,
                {moduleKnob, moduleSwitch.top + 4, moduleKnob + 24, moduleSwitch.bottom - 4},
                14,
                kWhite);

            Fill(device, {238, top + 82, client.right - 52, top + 83}, RGB(49, 55, 65));
            DrawLabel(device, L"Keybind", {238, top + 94, 330, top + 132}, kWhite, bodyFont);
            FillRounded(device, {330, top + 98, 370, top + 130}, 7, RGB(50, 57, 68));
            DrawLabel(
                device,
                L"C",
                {330, top + 98, 370, top + 130},
                kWhite,
                bodyFont,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            const bool bindEnabled = zoom_.IsKeybindEnabled();
            DrawLabel(
                device,
                bindEnabled ? L"Enabled" : L"Disabled",
                {388, top + 94, client.right - 130, top + 132},
                bindEnabled ? kAccent : kMuted,
                smallFont);
            const RECT bindSwitch{client.right - 112, top + 97, client.right - 52, top + 129};
            FillRounded(device, bindSwitch, 18, bindEnabled ? kAccent : RGB(63, 70, 82));
            const int bindKnob = bindEnabled ? bindSwitch.right - 28 : bindSwitch.left + 4;
            FillRounded(
                device,
                {bindKnob, bindSwitch.top + 4, bindKnob + 24, bindSwitch.bottom - 4},
                14,
                kWhite);

            const int nightTop = 280;
            const RECT nightCard{216, nightTop, client.right - 32, nightTop + 82};
            FillRounded(device, nightCard, 12, kCard);
            DrawLabel(
                device,
                L"Night Vision",
                {238, nightTop + 10, client.right - 150, nightTop + 40},
                kWhite,
                titleFont);
            const wchar_t* nightStatusText = L"Unavailable: game signatures did not validate.";
            COLORREF nightStatusColor = RGB(218, 117, 104);
            switch (nightVision_.Status())
            {
            case pebble::NightVisionStatus::Idle:
                nightStatusText = L"Ready. Toggle to apply the effect.";
                nightStatusColor = kMuted;
                break;
            case pebble::NightVisionStatus::WaitingForPlayer:
                nightStatusText = L"Waiting for local player — rejoin the world.";
                nightStatusColor = RGB(224, 189, 108);
                break;
            case pebble::NightVisionStatus::NoComponent:
                nightStatusText = L"Player found, but effect component not resolved.";
                nightStatusColor = RGB(218, 117, 104);
                break;
            case pebble::NightVisionStatus::Applied:
                nightStatusText = L"Applied ✓ effect written to memory.";
                nightStatusColor = RGB(126, 199, 138);
                break;
            case pebble::NightVisionStatus::Unavailable:
            default:
                break;
            }
            DrawLabel(
                device,
                nightStatusText,
                {238, nightTop + 40, client.right - 150, nightTop + 68},
                nightStatusColor,
                smallFont);
            const bool nightEnabled = nightVision_.IsEnabled();
            const RECT nightSwitch{
                client.right - 112,
                nightTop + 25,
                client.right - 52,
                nightTop + 57
            };
            FillRounded(
                device,
                nightSwitch,
                18,
                nightEnabled ? kAccent : RGB(63, 70, 82));
            const int nightKnob = nightEnabled
                ? nightSwitch.right - 28
                : nightSwitch.left + 4;
            FillRounded(
                device,
                {nightKnob, nightSwitch.top + 4, nightKnob + 24, nightSwitch.bottom - 4},
                14,
                kWhite);
        }
        else if (activeCategory_ == Category::Combat)
        {
            const CardFonts fonts{titleFont, smallFont};
            const COLORREF pending = RGB(224, 189, 108);
            DrawToggleCard(
                device, client, 106,
                L"Auto Armor",
                L"Pending: needs inventory memory hooks.",
                pending,
                autoArmor_.IsEnabled(),
                fonts);
            DrawToggleCard(
                device, client, 198,
                L"Triggerbot",
                L"Pending: needs entity hit-test memory hooks.",
                pending,
                triggerbot_.IsEnabled(),
                fonts);
        }
        else if (activeCategory_ == Category::Player)
        {
            const CardFonts fonts{titleFont, smallFont};

            // Auto Clicker: taller card with a clicks-per-second stepper.
            const int top = 106;
            const RECT card{216, top, client.right - 32, top + 154};
            FillRounded(device, card, 12, kCard);
            DrawLabel(device, L"Auto Clicker", {238, top + 12, client.right - 150, top + 42}, kWhite, titleFont);
            DrawLabel(
                device,
                L"Sends left clicks while active (menu closed).",
                {238, top + 42, client.right - 150, top + 68},
                kMuted,
                smallFont);

            const bool clickerEnabled = autoClicker_.IsEnabled();
            const RECT clickerSwitch{client.right - 112, top + 20, client.right - 52, top + 52};
            FillRounded(device, clickerSwitch, 18, clickerEnabled ? kAccent : RGB(63, 70, 82));
            const int clickerKnob = clickerEnabled ? clickerSwitch.right - 28 : clickerSwitch.left + 4;
            FillRounded(device, {clickerKnob, clickerSwitch.top + 4, clickerKnob + 24, clickerSwitch.bottom - 4}, 14, kWhite);

            Fill(device, {238, top + 82, client.right - 52, top + 83}, RGB(49, 55, 65));
            DrawLabel(device, L"Clicks / sec", {238, top + 94, 360, top + 132}, kWhite, bodyFont);

            // Stepper: [ - ] [ value ] [ + ] anchored to the right.
            const RECT minusBox{client.right - 158, top + 97, client.right - 126, top + 129};
            const RECT plusBox{client.right - 84, top + 97, client.right - 52, top + 129};
            FillRounded(device, minusBox, 7, RGB(50, 57, 68));
            FillRounded(device, plusBox, 7, RGB(50, 57, 68));
            DrawLabel(device, L"\x2013", minusBox, kWhite, titleFont, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawLabel(device, L"+", plusBox, kWhite, titleFont, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            wchar_t cpsText[8];
            wsprintfW(cpsText, L"%d", autoClicker_.ClicksPerSecond());
            DrawLabel(device, cpsText, {client.right - 126, top + 97, client.right - 84, top + 129}, kAccent, titleFont, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Auto Break: simple toggle card below.
            DrawToggleCard(
                device, client, 280,
                L"Auto Break",
                L"Holds attack to break blocks continuously.",
                kMuted,
                autoBreak_.IsEnabled(),
                fonts);
        }

        DeleteObject(smallFont);
        DeleteObject(bodyFont);
        DeleteObject(titleFont);
        DeleteObject(logoFont);
        EndPaint(window_, &paint);
    }

    void ClientMenu::HandleClick(int x, int y)
    {
        RECT client{};
        GetClientRect(window_, &client);
        if (x >= client.right - 60 && y <= 60)
        {
            Toggle();
            return;
        }

        if (x >= 16 && x <= 168 && y >= 112 && y <= 158)
        {
            activeCategory_ = Category::Cosmetics;
            InvalidateRect(window_, nullptr, FALSE);
            return;
        }
        if (x >= 16 && x <= 168 && y >= 166 && y <= 212)
        {
            activeCategory_ = Category::Render;
            InvalidateRect(window_, nullptr, FALSE);
            return;
        }
        if (x >= 16 && x <= 168 && y >= 220 && y <= 266)
        {
            activeCategory_ = Category::Combat;
            InvalidateRect(window_, nullptr, FALSE);
            return;
        }
        if (x >= 16 && x <= 168 && y >= 274 && y <= 320)
        {
            activeCategory_ = Category::Player;
            InvalidateRect(window_, nullptr, FALSE);
            return;
        }

        if (activeCategory_ == Category::Cosmetics)
        {
            for (std::size_t index = 0; index < menuCosmetics_.size(); ++index)
            {
                const int top = 106 + static_cast<int>(index) * 104;
                if (x >= 216 && x <= client.right - 32 && y >= top && y <= top + 82)
                {
                    const bool enabled = cosmetics_.IsEnabled(menuCosmetics_[index].id);
                    cosmetics_.SetEnabled(menuCosmetics_[index].id, !enabled);
                    InvalidateRect(window_, nullptr, FALSE);
                    return;
                }
            }
        }
        else if (activeCategory_ == Category::Render)
        {
            if (x >= 216 && x <= client.right - 32 && y >= 106 && y <= 178)
            {
                zoom_.SetEnabled(!zoom_.IsEnabled());
                InvalidateRect(window_, nullptr, FALSE);
            }
            else if (x >= 216 && x <= client.right - 32 && y >= 188 && y <= 260)
            {
                zoom_.SetKeybindEnabled(!zoom_.IsKeybindEnabled());
                InvalidateRect(window_, nullptr, FALSE);
            }
            else if (x >= 216 && x <= client.right - 32 && y >= 280 && y <= 362)
            {
                nightVision_.SetEnabled(!nightVision_.IsEnabled());
                InvalidateRect(window_, nullptr, FALSE);
            }
        }
        else if (activeCategory_ == Category::Combat)
        {
            if (HitCard(client, x, y, 106))
            {
                autoArmor_.SetEnabled(!autoArmor_.IsEnabled());
                InvalidateRect(window_, nullptr, FALSE);
            }
            else if (HitCard(client, x, y, 198))
            {
                triggerbot_.SetEnabled(!triggerbot_.IsEnabled());
                InvalidateRect(window_, nullptr, FALSE);
            }
        }
        else if (activeCategory_ == Category::Player)
        {
            const int top = 106;
            const bool inStepperRow = x >= 216 && x <= client.right - 32 &&
                y >= top + 97 && y <= top + 129;
            if (inStepperRow && x >= client.right - 158 && x <= client.right - 126)
            {
                autoClicker_.AdjustClicksPerSecond(-1);
                InvalidateRect(window_, nullptr, FALSE);
            }
            else if (inStepperRow && x >= client.right - 84 && x <= client.right - 52)
            {
                autoClicker_.AdjustClicksPerSecond(1);
                InvalidateRect(window_, nullptr, FALSE);
            }
            else if (x >= 216 && x <= client.right - 32 && y >= top && y <= top + 82)
            {
                autoClicker_.SetEnabled(!autoClicker_.IsEnabled());
                InvalidateRect(window_, nullptr, FALSE);
            }
            else if (HitCard(client, x, y, 280))
            {
                autoBreak_.SetEnabled(!autoBreak_.IsEnabled());
                InvalidateRect(window_, nullptr, FALSE);
            }
        }
    }

    void ClientMenu::UpdateBounds()
    {
        if (gameWindow_ == nullptr || !IsWindow(gameWindow_))
        {
            gameWindow_ = FindGameWindow();
        }

        RECT host{};
        if (gameWindow_ != nullptr && GetWindowRect(gameWindow_, &host))
        {
            const int hostWidth = host.right - host.left;
            const int hostHeight = host.bottom - host.top;
            const int width = std::min(kPreferredWidth, std::max(520, hostWidth - 40));
            const int height = std::min(kPreferredHeight, std::max(380, hostHeight - 40));
            SetWindowPos(
                window_,
                HWND_TOPMOST,
                host.left + (hostWidth - width) / 2,
                host.top + (hostHeight - height) / 2,
                width,
                height,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
            return;
        }

        RECT workArea{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        SetWindowPos(
            window_,
            HWND_TOPMOST,
            workArea.left + (workArea.right - workArea.left - kPreferredWidth) / 2,
            workArea.top + (workArea.bottom - workArea.top - kPreferredHeight) / 2,
            kPreferredWidth,
            kPreferredHeight,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    HWND ClientMenu::FindGameWindow() const
    {
        HWND result = nullptr;
        EnumWindows(FindGameWindowCallback, reinterpret_cast<LPARAM>(&result));
        return result;
    }
}
