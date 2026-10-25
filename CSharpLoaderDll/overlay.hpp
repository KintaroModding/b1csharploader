#pragma once

// In-game console: a click-through window on top of the game that shows the last log lines.
// The OverlayKey (F10 by default) shows and hides it while the game window has the focus.

enum BannerLevel {
    BannerInfo = 0,
    BannerWarning = 1,
    BannerError = 2,
};

void overlay_start(HMODULE module, const char* keyName);
const wchar_t* overlay_key_name();
void overlay_add_line(const wchar_t* line);
// Shows a banner at the top of the game for 5 seconds, lines are separated by '\n'
void overlay_show_banner(const wchar_t* text, int level);
// Lines under the console header, separated by '\n', lines that start with '!' show in red
void overlay_set_info(const wchar_t* text);

// Called by CSharpManager
extern "C" __declspec(dllexport) void CSharpLoader_ShowBanner(const wchar_t* text, int level);
extern "C" __declspec(dllexport) void CSharpLoader_SetConsoleInfo(const wchar_t* text);
extern "C" __declspec(dllexport) const wchar_t* CSharpLoader_GetOverlayKey();
