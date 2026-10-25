#include "pch.h"
#include "log.hpp"
#include "overlay.hpp"
#include <iterator>
#include <stdarg.h>
#include <stdio.h>

static void write_line(FILE* stream, char level, const char* fmt, va_list args) {
    char message[1024];
    vsnprintf_s(message, _TRUNCATE, fmt, args);
    SYSTEMTIME t;
    GetLocalTime(&t);
    char line[1100];
    _snprintf_s(line, _TRUNCATE, "%02d-%02d %02d:%02d:%02d [%c] %s", t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, level, message);
    fprintf(stream, "%s\n", line);
    fflush(stream);
    wchar_t wideLine[1100];
    if (MultiByteToWideChar(CP_ACP, 0, line, -1, wideLine, static_cast<int>(std::size(wideLine))) > 0) {
        overlay_add_line(wideLine);
    }
}

static void write_line(FILE* stream, wchar_t level, const wchar_t* fmt, va_list args) {
    wchar_t message[1024];
    _vsnwprintf_s(message, _TRUNCATE, fmt, args);
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[1100];
    _snwprintf_s(line, _TRUNCATE, L"%02d-%02d %02d:%02d:%02d [%c] %s", t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, level, message);
    fwprintf(stream, L"%s\n", line);
    fflush(stream);
    overlay_add_line(line);
}

void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_line(stdout, 'I', fmt, args);
    va_end(args);
}

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_line(stderr, 'E', fmt, args);
    va_end(args);
}

void log_info(const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_line(stdout, L'I', fmt, args);
    va_end(args);
}

void log_error(const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_line(stderr, L'E', fmt, args);
    va_end(args);
}
