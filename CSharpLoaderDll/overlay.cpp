#include "pch.h"
#include "overlay.hpp"
#include "log.hpp"
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

namespace {
    constexpr size_t MaxLines = 500;
    constexpr BYTE WindowAlpha = 215;
    constexpr UINT_PTR TimerId = 1;
    constexpr UINT TimerMs = 50;
    constexpr ULONGLONG BannerMs = 5000;
    constexpr ULONGLONG BannerFadeMs = 600;
    constexpr COLORREF AccentColor = RGB(120, 160, 220);
    constexpr COLORREF WarningColor = RGB(255, 215, 90);
    constexpr COLORREF ErrorColor = RGB(255, 100, 100);
    const wchar_t ClassName[] = L"CSharpLoaderOverlay";

    struct NamedKey {
        int key;
        const wchar_t* name;
    };
    // keys that can open the console, besides F1-F24, A-Z, 0-9 and virtual key codes like 0xC0
    const NamedKey NamedKeys[] = {
        { VK_INSERT, L"Insert" }, { VK_DELETE, L"Delete" }, { VK_HOME, L"Home" }, { VK_END, L"End" },
        { VK_PAUSE, L"Pause" }, { VK_SCROLL, L"ScrollLock" }, { VK_OEM_3, L"Tilde" },
    };

    // a small box on top of the game with a colored bar on the left: the banner and the error marker
    struct Toast {
        HWND hwnd = nullptr;
        std::vector<std::wstring> lines;
        bool hasTitle = false;  // the first line uses the big font
        COLORREF accent = AccentColor;
        RECT rect{};
    };

    std::mutex stateMutex;
    std::deque<std::wstring> lines;       // guarded by stateMutex
    unsigned long long lineCount = 0;     // all lines ever added, guarded by stateMutex
    int unseenErrors = 0;                 // error lines while the console is closed, guarded by stateMutex
    std::vector<std::wstring> infoLines;  // shown under the console header, guarded by stateMutex
    bool consoleChanged = false;          // guarded by stateMutex
    std::wstring bannerText;              // guarded by stateMutex
    int bannerLevel = BannerInfo;         // guarded by stateMutex
    bool bannerPending = false;           // guarded by stateMutex

    // set before the overlay thread starts
    int toggleKey = VK_F10;
    std::wstring toggleKeyName = L"F10";

    // only used on the overlay thread
    HWND consoleHwnd = nullptr;
    HWND gameHwnd = nullptr;
    Toast banner;
    Toast marker;
    bool consoleOpen = false;
    bool toggleWasDown = false;
    bool pageUpWasDown = false;
    bool pageDownWasDown = false;
    size_t scrollOffset = 0;  // lines from the newest one, 0 follows new lines
    size_t pageLines = 1;     // log lines that fit into the console
    unsigned long long seenLineCount = 0;
    int markerErrors = 0;
    HFONT font = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    int consoleFontHeight = 0;
    int bannerFontHeight = 0;
    RECT consoleRect{};
    ULONGLONG bannerUntil = 0;
    BYTE bannerAlpha = WindowAlpha;

    // returns the virtual key code and sets displayName, 0 when the name is unknown
    int parse_key(const std::wstring& name, std::wstring& displayName) {
        wchar_t* end = nullptr;
        if (name.size() >= 2 && towupper(name[0]) == L'F') {
            const long number = wcstol(name.c_str() + 1, &end, 10);
            if (*end == L'\0' && number >= 1 && number <= 24) {
                displayName = L"F" + std::to_wstring(number);
                return VK_F1 + number - 1;
            }
        }
        if (name.size() == 1) {
            const wchar_t c = towupper(name[0]);
            if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
                displayName = std::wstring(1, c);
                return c;
            }
        }
        for (const auto& named : NamedKeys) {
            if (_wcsicmp(name.c_str(), named.name) == 0) {
                displayName = named.name;
                return named.key;
            }
        }
        if (name.size() > 2 && name[0] == L'0' && towupper(name[1]) == L'X') {
            const long code = wcstol(name.c_str() + 2, &end, 16);
            // Page Up and Page Down scroll the console
            if (*end == L'\0' && code > 0 && code < 0xFF && code != VK_PRIOR && code != VK_NEXT) {
                displayName = name;
                return code;
            }
        }
        return 0;
    }

    BOOL CALLBACK find_game_window_proc(HWND hwnd, LPARAM lParam) {
        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);
        wchar_t className[64];
        if (processId == GetCurrentProcessId() &&
            GetClassNameW(hwnd, className, static_cast<int>(std::size(className))) > 0 &&
            wcscmp(className, L"UnrealWindow") == 0) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }
        return TRUE;  // continue enumerating
    }

    HWND find_game_window() {
        HWND found = nullptr;
        EnumWindows(find_game_window_proc, reinterpret_cast<LPARAM>(&found));
        return found;
    }

    // lines look like "MM-dd HH:mm:ss [E] message"
    wchar_t line_level(const std::wstring& line) {
        return line.size() > 17 && line[15] == L'[' && line[17] == L']' ? line[16] : L'\0';
    }

    COLORREF line_color(const std::wstring& line) {
        switch (line_level(line)) {
        case L'E': return ErrorColor;
        case L'W': return WarningColor;
        case L'D': return RGB(160, 160, 160);
        }
        return RGB(230, 230, 230);
    }

    COLORREF level_color(int level) {
        switch (level) {
        case BannerError: return ErrorColor;
        case BannerWarning: return WarningColor;
        }
        return AccentColor;
    }

    std::vector<std::wstring> split_lines(const std::wstring& text) {
        std::vector<std::wstring> result;
        size_t start = 0;
        for (size_t end = text.find(L'\n'); end != std::wstring::npos; end = text.find(L'\n', start)) {
            result.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        result.push_back(text.substr(start));
        return result;
    }

    bool key_pressed(int key, bool& wasDown) {
        const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
        const bool pressed = down && !wasDown;
        wasDown = down;
        return pressed;
    }

    HFONT create_font(int height, int weight, DWORD pitchAndFamily, const wchar_t* face) {
        return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, pitchAndFamily, face);
    }

    // the text sizes follow the game's height
    void update_fonts(int clientHeight) {
        const int consoleHeight = (std::max)(8, clientHeight * 7 / 540 - 2);  // 12 px at 1080p
        const int bannerHeight = (std::max)(14, clientHeight / 54);           // 20 px at 1080p
        if (font != nullptr && consoleHeight == consoleFontHeight && bannerHeight == bannerFontHeight) {
            return;
        }
        for (HFONT old : { font, titleFont, textFont }) {
            if (old != nullptr) {
                DeleteObject(old);
            }
        }
        consoleFontHeight = consoleHeight;
        bannerFontHeight = bannerHeight;
        font = create_font(consoleHeight, FW_NORMAL, FIXED_PITCH | FF_MODERN, L"Consolas");
        titleFont = create_font(bannerHeight * 7 / 5, FW_SEMIBOLD, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
        textFont = create_font(bannerHeight, FW_NORMAL, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
        // the toasts need a new size
        banner.rect = {};
        marker.rect = {};
    }

    int toast_bar_width() {
        return (std::max)(3, bannerFontHeight / 5);
    }

    // measures the toast, and draws it when draw is true
    SIZE toast_layout(HDC dc, const Toast& toast, bool draw) {
        const int padding = toast.hasTitle ? bannerFontHeight : bannerFontHeight * 2 / 3;
        const int x = toast_bar_width() + padding;
        int y = padding * 3 / 4;
        int width = 0;
        for (size_t i = 0; i < toast.lines.size(); i++) {
            const std::wstring& line = toast.lines[i];
            SelectObject(dc, toast.hasTitle && i == 0 ? titleFont : textFont);
            SIZE extent{};
            GetTextExtentPoint32W(dc, line.c_str(), static_cast<int>(line.size()), &extent);
            if (draw) {
                SetTextColor(dc, i == 0 ? RGB(240, 240, 240) : RGB(175, 175, 175));
                TextOutW(dc, x, y, line.c_str(), static_cast<int>(line.size()));
            }
            width = (std::max)(width, static_cast<int>(extent.cx));
            y += extent.cy;
        }
        return SIZE{ x + width + padding, y + padding * 3 / 4 };
    }

    SIZE measure_toast(const Toast& toast) {
        HDC dc = GetDC(toast.hwnd);
        HGDIOBJ oldFont = GetCurrentObject(dc, OBJ_FONT);
        const SIZE size = toast_layout(dc, toast, false);
        SelectObject(dc, oldFont);
        ReleaseDC(toast.hwnd, dc);
        return size;
    }

    void draw_toast(HDC dc, const RECT& rc, const Toast& toast) {
        RECT bar{ 0, 0, toast_bar_width(), rc.bottom };
        HBRUSH accent = CreateSolidBrush(toast.accent);
        FillRect(dc, &bar, accent);
        DeleteObject(accent);
        toast_layout(dc, toast, true);
    }

    void draw_text_line(HDC dc, const std::wstring& text, int x, int y, int right, int lineHeight) {
        RECT lineRect{ x, y, right, y + lineHeight };
        ExtTextOutW(dc, x, y, ETO_CLIPPED, &lineRect, text.c_str(), static_cast<UINT>(text.size()), nullptr);
    }

    void draw_console(HDC dc, const RECT& rc) {
        SelectObject(dc, font);
        TEXTMETRICW tm;
        GetTextMetricsW(dc, &tm);
        const int lineHeight = (std::max)(1, static_cast<int>(tm.tmHeight));
        const int padding = lineHeight / 2;

        std::vector<std::wstring> info;
        std::vector<std::wstring> visibleLines;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            info = infoLines;
            const int top = padding + lineHeight * static_cast<int>(1 + info.size()) + padding;
            const size_t maxLines = static_cast<size_t>((std::max)(0, (static_cast<int>(rc.bottom) - top - padding) / lineHeight));
            pageLines = (std::max)(static_cast<size_t>(1), maxLines);
            const size_t maxOffset = lines.size() > maxLines ? lines.size() - maxLines : 0;
            scrollOffset = (std::min)(scrollOffset, maxOffset);
            const size_t end = lines.size() - scrollOffset;
            const size_t begin = end - (std::min)(end, maxLines);
            visibleLines.assign(lines.begin() + begin, lines.begin() + end);
        }

        int y = padding;
        std::wstring header = L"C# Loader console  \u00B7  " + toggleKeyName + L" closes  \u00B7  Page Up / Page Down scroll";
        if (scrollOffset > 0) {
            header += L"  \u00B7  scrolled up";
        }
        SetTextColor(dc, AccentColor);
        draw_text_line(dc, header, padding, y, rc.right - padding, lineHeight);
        y += lineHeight;
        for (const auto& line : info) {
            // info lines that start with '!' show in red
            const bool error = !line.empty() && line[0] == L'!';
            SetTextColor(dc, error ? ErrorColor : RGB(175, 175, 175));
            draw_text_line(dc, error ? line.substr(1) : line, padding, y, rc.right - padding, lineHeight);
            y += lineHeight;
        }
        y += padding;
        for (const auto& line : visibleLines) {
            SetTextColor(dc, line_color(line));
            draw_text_line(dc, line, padding, y, rc.right - padding, lineHeight);
            y += lineHeight;
        }
    }

    void paint(HWND hwnd) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // draw into a memory bitmap first, so the text does not flicker
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBitmap = SelectObject(mem, bitmap);
        HGDIOBJ oldFont = GetCurrentObject(mem, OBJ_FONT);
        FillRect(mem, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetBkMode(mem, TRANSPARENT);
        if (hwnd == banner.hwnd) {
            draw_toast(mem, rc, banner);
        } else if (hwnd == marker.hwnd) {
            draw_toast(mem, rc, marker);
        } else {
            draw_console(mem, rc);
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldFont);
        SelectObject(mem, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
    }

    void hide(HWND hwnd) {
        if (IsWindowVisible(hwnd)) {
            ShowWindow(hwnd, SW_HIDE);
        }
    }

    // moves the window to target and shows it, returns true when it needs a redraw
    bool place(HWND hwnd, const RECT& target, RECT& current) {
        bool redraw = false;
        if (!EqualRect(&target, &current)) {
            current = target;
            SetWindowPos(hwnd, HWND_TOPMOST, target.left, target.top,
                target.right - target.left, target.bottom - target.top, SWP_NOACTIVATE);
            redraw = true;
        }
        if (!IsWindowVisible(hwnd)) {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            redraw = true;
        }
        return redraw;
    }

    // area: the game's client area on the screen, nullptr when nothing may show
    void update_console(const RECT* area, bool changed) {
        if (!consoleOpen || area == nullptr) {
            hide(consoleHwnd);
            return;
        }
        // the top 40% of the game
        RECT target{ area->left, area->top, area->right, area->top + (area->bottom - area->top) * 2 / 5 };
        if (place(consoleHwnd, target, consoleRect) || changed) {
            InvalidateRect(consoleHwnd, nullptr, FALSE);
        }
    }

    void update_banner(const RECT* area) {
        const ULONGLONG now = GetTickCount64();
        if (area != nullptr) {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (bannerPending) {
                // the 5 seconds start when the banner can show
                bannerPending = false;
                banner.lines = split_lines(bannerText);
                banner.accent = level_color(bannerLevel);
                banner.rect = {};
                bannerUntil = now + BannerMs;
            }
        }
        if (area == nullptr || consoleOpen || now >= bannerUntil) {
            hide(banner.hwnd);
            return;
        }

        // top center of the game
        const SIZE size = measure_toast(banner);
        const int x = area->left + (area->right - area->left - size.cx) / 2;
        const int y = area->top + (area->bottom - area->top) / 16;
        RECT target{ x, y, x + size.cx, y + size.cy };
        const bool redraw = place(banner.hwnd, target, banner.rect);

        // fade out at the end
        const ULONGLONG remaining = bannerUntil - now;
        const BYTE alpha = remaining < BannerFadeMs ? static_cast<BYTE>(WindowAlpha * remaining / BannerFadeMs) : WindowAlpha;
        if (alpha != bannerAlpha) {
            bannerAlpha = alpha;
            SetLayeredWindowAttributes(banner.hwnd, 0, alpha, LWA_ALPHA);
        }
        if (redraw) {
            InvalidateRect(banner.hwnd, nullptr, FALSE);
        }
    }

    // a small red box in the top right corner while there are errors the player has not seen
    void update_marker(const RECT* area, int errors) {
        if (area == nullptr || consoleOpen || errors == 0) {
            hide(marker.hwnd);
            return;
        }
        if (errors != markerErrors) {
            markerErrors = errors;
            marker.lines = { errors == 1
                ? L"1 new error  \u00B7  " + toggleKeyName + L" shows it"
                : std::to_wstring(errors) + L" new errors  \u00B7  " + toggleKeyName + L" shows them" };
            marker.rect = {};
        }
        const SIZE size = measure_toast(marker);
        const int margin = (area->bottom - area->top) / 40;
        RECT target{ area->right - margin - size.cx, area->top + margin, area->right - margin, area->top + margin + size.cy };
        if (place(marker.hwnd, target, marker.rect)) {
            InvalidateRect(marker.hwnd, nullptr, FALSE);
        }
    }

    // runs every TimerMs on the overlay thread
    void update() {
        if (gameHwnd == nullptr || !IsWindow(gameHwnd)) {
            gameHwnd = find_game_window();
        }
        const bool focused = gameHwnd != nullptr && GetForegroundWindow() == gameHwnd && !IsIconic(gameHwnd);
        const bool togglePressed = key_pressed(toggleKey, toggleWasDown);
        const bool pageUpPressed = key_pressed(VK_PRIOR, pageUpWasDown);
        const bool pageDownPressed = key_pressed(VK_NEXT, pageDownWasDown);

        bool changed = false;
        if (togglePressed && focused) {
            consoleOpen = !consoleOpen;
            scrollOffset = 0;
        }
        if (consoleOpen && focused && pageUpPressed) {
            scrollOffset += pageLines;
            changed = true;
        }
        if (consoleOpen && focused && pageDownPressed) {
            scrollOffset = scrollOffset > pageLines ? scrollOffset - pageLines : 0;
            changed = true;
        }

        int errors = 0;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            // keep the view in place while scrolled up
            if (scrollOffset > 0) {
                scrollOffset += static_cast<size_t>(lineCount - seenLineCount);
            }
            seenLineCount = lineCount;
            if (consoleOpen) {
                unseenErrors = 0;
            }
            errors = unseenErrors;
            changed = changed || consoleChanged;
            consoleChanged = false;
        }

        RECT area{};
        bool hasArea = false;
        if (focused) {
            RECT client;
            POINT origin{ 0, 0 };
            if (GetClientRect(gameHwnd, &client) && ClientToScreen(gameHwnd, &origin) && client.right > 0 && client.bottom > 0) {
                area = { origin.x, origin.y, origin.x + client.right, origin.y + client.bottom };
                hasArea = true;
                update_fonts(client.bottom);
            }
        }
        update_console(hasArea ? &area : nullptr, changed);
        update_banner(hasArea ? &area : nullptr);
        update_marker(hasArea ? &area : nullptr, errors);
    }

    LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            update();
            return 0;
        case WM_PAINT:
            paint(hwnd);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    HWND create_overlay_window(HINSTANCE instance) {
        // click-through, never takes the focus, not in the taskbar
        HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            ClassName, ClassName, WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
        if (hwnd != nullptr) {
            SetLayeredWindowAttributes(hwnd, 0, WindowAlpha, LWA_ALPHA);
        }
        return hwnd;
    }

    DWORD WINAPI overlay_thread(LPVOID module) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = window_proc;
        wc.hInstance = static_cast<HINSTANCE>(module);
        wc.lpszClassName = ClassName;
        if (RegisterClassExW(&wc) == 0) {
            return EXIT_FAILURE;
        }
        consoleHwnd = create_overlay_window(wc.hInstance);
        banner.hwnd = create_overlay_window(wc.hInstance);
        banner.hasTitle = true;
        marker.hwnd = create_overlay_window(wc.hInstance);
        marker.accent = ErrorColor;
        if (consoleHwnd == nullptr || banner.hwnd == nullptr || marker.hwnd == nullptr) {
            return EXIT_FAILURE;
        }
        SetTimer(consoleHwnd, TimerId, TimerMs, nullptr);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return EXIT_SUCCESS;
    }
}

void overlay_start(HMODULE module, const char* keyName) {
    const std::wstring name(keyName, keyName + strlen(keyName));
    std::wstring displayName;
    const int key = parse_key(name, displayName);
    if (key != 0) {
        toggleKey = key;
        toggleKeyName = displayName;
    } else {
        log_error("OverlayKey \"%s\" is unknown, using F10.", keyName);
    }
    HANDLE thread = CreateThread(nullptr, 0, overlay_thread, module, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
}

const wchar_t* overlay_key_name() {
    return toggleKeyName.c_str();
}

void overlay_add_line(const wchar_t* line) {
    std::wstring text(line);
    // tabs would show as boxes
    for (size_t pos = text.find(L'\t'); pos != std::wstring::npos; pos = text.find(L'\t', pos)) {
        text.replace(pos, 1, L"    ");
    }
    const bool error = line_level(text) == L'E';
    std::lock_guard<std::mutex> lock(stateMutex);
    lines.push_back(std::move(text));
    if (lines.size() > MaxLines) {
        lines.pop_front();
    }
    lineCount++;
    if (error) {
        unseenErrors++;
    }
    consoleChanged = true;
}

void overlay_show_banner(const wchar_t* text, int level) {
    std::lock_guard<std::mutex> lock(stateMutex);
    bannerText = text;
    bannerLevel = level;
    bannerPending = true;
}

void overlay_set_info(const wchar_t* text) {
    std::vector<std::wstring> info = text[0] == L'\0' ? std::vector<std::wstring>() : split_lines(text);
    std::lock_guard<std::mutex> lock(stateMutex);
    infoLines = std::move(info);
    consoleChanged = true;
}

extern "C" __declspec(dllexport) void CSharpLoader_ShowBanner(const wchar_t* text, int level) {
    if (text != nullptr) {
        overlay_show_banner(text, level);
    }
}

extern "C" __declspec(dllexport) void CSharpLoader_SetConsoleInfo(const wchar_t* text) {
    if (text != nullptr) {
        overlay_set_info(text);
    }
}

extern "C" __declspec(dllexport) const wchar_t* CSharpLoader_GetOverlayKey() {
    return overlay_key_name();
}
