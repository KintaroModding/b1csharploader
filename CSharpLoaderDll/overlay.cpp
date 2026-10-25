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
    const wchar_t ClassName[] = L"CSharpLoaderOverlay";
    const wchar_t Header[] = L"C# Loader console  (F10 to close)";

    std::mutex linesMutex;
    std::deque<std::wstring> lines;
    bool linesChanged = false;  // guarded by linesMutex

    // only used on the overlay thread
    HWND overlayHwnd = nullptr;
    HWND gameHwnd = nullptr;
    bool visible = false;
    bool keyWasDown = false;
    HFONT font = nullptr;
    int fontHeight = 0;
    RECT overlayRect{};

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

    void update_font(int clientHeight) {
        int height = (std::max)(14, clientHeight / 54);
        if (font != nullptr && height == fontHeight) {
            return;
        }
        if (font != nullptr) {
            DeleteObject(font);
        }
        fontHeight = height;
        font = CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }

    void paint(HWND hwnd) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        const int width = rc.right;
        const int height = rc.bottom;

        // draw into a memory bitmap first, so the text does not flicker
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
        HGDIOBJ oldBitmap = SelectObject(mem, bitmap);
        HGDIOBJ oldFont = SelectObject(mem, font);
        FillRect(mem, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetBkMode(mem, TRANSPARENT);

        TEXTMETRICW tm;
        GetTextMetricsW(mem, &tm);
        const int lineHeight = (std::max)(1, static_cast<int>(tm.tmHeight));
        const int padding = lineHeight / 2;
        int y = padding;

        SetTextColor(mem, RGB(120, 160, 220));
        TextOutW(mem, padding, y, Header, static_cast<int>(wcslen(Header)));
        y += lineHeight + padding;

        const int maxLines = (std::max)(0, (height - y - padding) / lineHeight);
        std::vector<std::wstring> visibleLines;
        {
            std::lock_guard<std::mutex> lock(linesMutex);
            const size_t count = (std::min)(lines.size(), static_cast<size_t>(maxLines));
            visibleLines.assign(lines.end() - count, lines.end());
        }
        for (const auto& line : visibleLines) {
            SetTextColor(mem, line_color(line));
            RECT lineRect{ padding, y, width - padding, y + lineHeight };
            ExtTextOutW(mem, padding, y, ETO_CLIPPED, &lineRect, line.c_str(), static_cast<UINT>(line.size()), nullptr);
            y += lineHeight;
        }

        BitBlt(hdc, 0, 0, width, height, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldFont);
        SelectObject(mem, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
    }

    void hide() {
        if (IsWindowVisible(overlayHwnd)) {
            ShowWindow(overlayHwnd, SW_HIDE);
        }
    }

    // runs every TimerMs on the overlay thread
    void update() {
        if (gameHwnd == nullptr || !IsWindow(gameHwnd)) {
            gameHwnd = find_game_window();
        }
        const bool focused = gameHwnd != nullptr && GetForegroundWindow() == gameHwnd;
        const bool keyDown = (GetAsyncKeyState(ToggleKey) & 0x8000) != 0;
        if (keyDown && !keyWasDown && focused) {
            visible = !visible;
        }
        keyWasDown = keyDown;

        if (!visible || !focused || IsIconic(gameHwnd)) {
            hide();
            return;
        }

        // cover the top 40% of the game's client area
        RECT client;
        GetClientRect(gameHwnd, &client);
        POINT origin{ 0, 0 };
        ClientToScreen(gameHwnd, &origin);
        RECT target{ origin.x, origin.y, origin.x + client.right, origin.y + client.bottom * 2 / 5 };
        if (target.right <= target.left || target.bottom <= target.top) {
            hide();
            return;
        }

        bool redraw = false;
        if (!EqualRect(&target, &overlayRect)) {
            overlayRect = target;
            update_font(client.bottom);
            SetWindowPos(overlayHwnd, HWND_TOPMOST, target.left, target.top,
                target.right - target.left, target.bottom - target.top, SWP_NOACTIVATE);
            redraw = true;
        }
        {
            std::lock_guard<std::mutex> lock(linesMutex);
            redraw = redraw || linesChanged;
            linesChanged = false;
        }
        if (!IsWindowVisible(overlayHwnd)) {
            SetWindowPos(overlayHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            redraw = true;
        }
        if (redraw) {
            InvalidateRect(overlayHwnd, nullptr, FALSE);
        }
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

    DWORD WINAPI overlay_thread(LPVOID module) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = window_proc;
        wc.hInstance = static_cast<HINSTANCE>(module);
        wc.lpszClassName = ClassName;
        if (RegisterClassExW(&wc) == 0) {
            return EXIT_FAILURE;
        }
        // click-through, never takes the focus, not in the taskbar
        overlayHwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            ClassName, ClassName, WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
        if (overlayHwnd == nullptr) {
            return EXIT_FAILURE;
        }
        SetLayeredWindowAttributes(overlayHwnd, 0, WindowAlpha, LWA_ALPHA);
        SetTimer(overlayHwnd, TimerId, TimerMs, nullptr);

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
    std::lock_guard<std::mutex> lock(linesMutex);
    lines.push_back(std::move(text));
    if (lines.size() > MaxLines) {
        lines.pop_front();
    }
    linesChanged = true;
}

extern "C" __declspec(dllexport) void CSharpLoader_LogLine(const wchar_t* line) {
    if (line != nullptr) {
        overlay_add_line(line);
    }
}
