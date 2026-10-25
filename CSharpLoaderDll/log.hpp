#pragma once

// Console log lines in the same format as CSharpModBase.Log: "MM-dd HH:mm:ss [I] message"
void log_info(const char* fmt, ...);
void log_error(const char* fmt, ...);
void log_info(const wchar_t* fmt, ...);
void log_error(const wchar_t* fmt, ...);
