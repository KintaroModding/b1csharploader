#pragma once

// In-game console: a click-through window on top of the game that shows the last log lines.
// F10 shows and hides it while the game window has the focus.
void overlay_start(HMODULE module);
void overlay_add_line(const wchar_t* line);

// Called by CSharpManager for every line written to Console.Out and Console.Error
extern "C" __declspec(dllexport) void CSharpLoader_LogLine(const wchar_t* line);
