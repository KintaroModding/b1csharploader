#include "pch.h"
#include "overlay.hpp"
#include <deque>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

namespace {
    constexpr size_t MaxLines = 500;
    constexpr int ToggleKey = VK_F10;
    constexpr BYTE WindowAlpha = 215;
    constexpr UINT_PTR TimerId = 1;
    constexpr UINT TimerMs = 50;
    constexpr ULONGLONG BannerMs = 5000;
    constexpr ULONGLONG BannerFadeMs = 600;
    constexpr COLORREF AccentColor = RGB(120, 160, 220);
    const wchar_t ClassName[] = L"CSharpLoaderOverlay";
    const wchar_t Header[] = L"C# Loader console  (F10 to close)";

    std::mutex stateMutex;
    std::deque<std::wstring> lines;   // guarded by stateMutex
    bool linesChanged = false;        // guarded by stateMutex
    std::wstring bannerText;          // guarded by stateMutex
    bool bannerPending = false;       // guarded by stateMutex

    // only used on the overlay thread
    HWND consoleHwnd = nullptr;
    HWND bannerHwnd = nullptr;
    HWND gameHwnd = nullptr;
    bool consoleOpen = false;
    bool keyWasDown = false;
    HFONT font = nullptr;
    HFONT titleFont = nullptr;
    HFONT textFont = nullptr;
    int fontHeight = 0;
    RECT consoleRect{};
    RECT bannerRect{};
    std::vector<std::wstring> bannerLines;
    ULONGLONG bannerUntil = 0;
    BYTE bannerAlpha = WindowAlpha;

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

    // lines look like "MM-dd HH:mm:ss [E] message", color them by level
    COLORREF line_color(const std::wstring& line) {
        if (line.size() > 17 && line[15] == L'[' && line[17] == L']') {
            switch (line[16]) {
            case L'E': return RGB(255, 100, 100);
            case L'W': return RGB(255, 215, 90);
            case L'D': return RGB(160, 160, 160);
            }
        }
        return RGB(230, 230, 230);
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

    HFONT create_font(int height, int weight, DWORD pitchAndFamily, const wchar_t* face) {
        return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, pitchAndFamily, face);
    }

    // the text size follows the game's height
    void update_fonts(int clientHeight) {
        const int height = (std::max)(14, clientHeight / 54);
        if (font != nullptr && height == fontHeight) {
            return;
        }
        for (HFONT old : { font, titleFont, textFont }) {
            if (old != nullptr) {
                DeleteObject(old);
            }
        }
        fontHeight = height;
        font = create_font(height, FW_NORMAL, FIXED_PITCH | FF_MODERN, L"Consolas");
        titleFont = create_font(height * 7 / 5, FW_SEMIBOLD, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
        textFont = create_font(height, FW_NORMAL, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
    }

    int banner_bar_width() {
        return (std::max)(3, fontHeight / 5);
    }

    // measures the banner, and draws it when draw is true
    SIZE banner_layout(HDC dc, bool draw) {
        const int padding = fontHeight;
        const int x = banner_bar_width() + padding;
        int y = padding * 3 / 4;
        int width = 0;
        for (size_t i = 0; i < bannerLines.size(); i++) {
            const std::wstring& line = bannerLines[i];
            SelectObject(dc, i == 0 ? titleFont : textFont);
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

    void draw_banner(HDC dc, const RECT& rc) {
        RECT bar{ 0, 0, banner_bar_width(), rc.bottom };
        HBRUSH accent = CreateSolidBrush(AccentColor);
        FillRect(dc, &bar, accent);
        DeleteObject(accent);
        banner_layout(dc, true);
    }

    void draw_console(HDC dc, const RECT& rc) {
        SelectObject(dc, font);
        TEXTMETRICW tm;
        GetTextMetricsW(dc, &tm);
        const int lineHeight = (std::max)(1, static_cast<int>(tm.tmHeight));
        const int padding = lineHeight / 2;
        int y = padding;

        SetTextColor(dc, AccentColor);
        TextOutW(dc, padding, y, Header, static_cast<int>(wcslen(Header)));
        y += lineHeight + padding;

        const int maxLines = (std::max)(0, (static_cast<int>(rc.bottom) - y - padding) / lineHeight);
        std::vector<std::wstring> visibleLines;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            const size_t count = (std::min)(lines.size(), static_cast<size_t>(maxLines));
            visibleLines.assign(lines.end() - count, lines.end());
        }
        for (const auto& line : visibleLines) {
            SetTextColor(dc, line_color(line));
            RECT lineRect{ padding, y, rc.right - padding, y + lineHeight };
            ExtTextOutW(dc, padding, y, ETO_CLIPPED, &lineRect, line.c_str(), static_cast<UINT>(line.size()), nullptr);
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
        if (hwnd == bannerHwnd) {
            draw_banner(mem, rc);
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
    void update_console(const RECT* area) {
        if (!consoleOpen || area == nullptr) {
            hide(consoleHwnd);
            return;
        }
        // the top 40% of the game
        RECT target{ area->left, area->top, area->right, area->top + (area->bottom - area->top) * 2 / 5 };
        bool redraw = place(consoleHwnd, target, consoleRect);
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            redraw = redraw || linesChanged;
            linesChanged = false;
        }
        if (redraw) {
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
                bannerLines = split_lines(bannerText);
                bannerUntil = now + BannerMs;
                bannerRect = {};
            }
        }
        if (area == nullptr || consoleOpen || now >= bannerUntil) {
            hide(bannerHwnd);
            return;
        }

        HDC dc = GetDC(bannerHwnd);
        HGDIOBJ oldFont = GetCurrentObject(dc, OBJ_FONT);
        const SIZE size = banner_layout(dc, false);
        SelectObject(dc, oldFont);
        ReleaseDC(bannerHwnd, dc);

        // top center of the game
        const int x = area->left + (area->right - area->left - size.cx) / 2;
        const int y = area->top + (area->bottom - area->top) / 16;
        RECT target{ x, y, x + size.cx, y + size.cy };
        const bool redraw = place(bannerHwnd, target, bannerRect);

        // fade out at the end
        const ULONGLONG remaining = bannerUntil - now;
        const BYTE alpha = remaining < BannerFadeMs ? static_cast<BYTE>(WindowAlpha * remaining / BannerFadeMs) : WindowAlpha;
        if (alpha != bannerAlpha) {
            bannerAlpha = alpha;
            SetLayeredWindowAttributes(bannerHwnd, 0, alpha, LWA_ALPHA);
        }
        if (redraw) {
            InvalidateRect(bannerHwnd, nullptr, FALSE);
        }
    }

    // runs every TimerMs on the overlay thread
    void update() {
        if (gameHwnd == nullptr || !IsWindow(gameHwnd)) {
            gameHwnd = find_game_window();
        }
        const bool focused = gameHwnd != nullptr && GetForegroundWindow() == gameHwnd && !IsIconic(gameHwnd);
        const bool keyDown = (GetAsyncKeyState(ToggleKey) & 0x8000) != 0;
        if (keyDown && !keyWasDown && focused) {
            consoleOpen = !consoleOpen;
        }
        keyWasDown = keyDown;

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
        update_console(hasArea ? &area : nullptr);
        update_banner(hasArea ? &area : nullptr);
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
        bannerHwnd = create_overlay_window(wc.hInstance);
        if (consoleHwnd == nullptr || bannerHwnd == nullptr) {
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

void overlay_start(HMODULE module) {
    HANDLE thread = CreateThread(nullptr, 0, overlay_thread, module, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
}

void overlay_add_line(const wchar_t* line) {
    std::wstring text(line);
    // tabs would show as boxes
    for (size_t pos = text.find(L'\t'); pos != std::wstring::npos; pos = text.find(L'\t', pos)) {
        text.replace(pos, 1, L"    ");
    }
    std::lock_guard<std::mutex> lock(stateMutex);
    lines.push_back(std::move(text));
    if (lines.size() > MaxLines) {
        lines.pop_front();
    }
    linesChanged = true;
}

void overlay_show_banner(const wchar_t* text) {
    std::lock_guard<std::mutex> lock(stateMutex);
    bannerText = text;
    bannerPending = true;
}

extern "C" __declspec(dllexport) void CSharpLoader_LogLine(const wchar_t* line) {
    if (line != nullptr) {
        overlay_add_line(line);
    }
}

extern "C" __declspec(dllexport) void CSharpLoader_ShowBanner(const wchar_t* text) {
    if (text != nullptr) {
        overlay_show_banner(text);
    }
}
