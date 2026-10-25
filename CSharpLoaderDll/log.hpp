#pragma once

// Console log lines in the same format as CSharpModBase.Log: "MM-dd HH:mm:ss [I] message"
void log_info(const char* fmt, ...);
void log_error(const char* fmt, ...);
void log_info(const wchar_t* fmt, ...);
void log_error(const wchar_t* fmt, ...);

// Adds a finished line to the log file in CSharpLoader\Logs and to the in-game console
void log_line(const wchar_t* line);

// Called by CSharpManager for every line written to Console.Out and Console.Error
extern "C" __declspec(dllexport) void CSharpLoader_LogLine(const wchar_t* line);
