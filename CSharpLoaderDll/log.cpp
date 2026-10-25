#include "pch.h"
#include "log.hpp"
#include <stdarg.h>
#include <stdio.h>

static void write_line(FILE* stream, char level, const char* fmt, va_list args) {
    char message[1024];
    vsnprintf_s(message, _TRUNCATE, fmt, args);
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(stream, "%02d-%02d %02d:%02d:%02d [%c] %s\n", t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, level, message);
    fflush(stream);
}

static void write_line(FILE* stream, wchar_t level, const wchar_t* fmt, va_list args) {
    wchar_t message[1024];
    _vsnwprintf_s(message, _TRUNCATE, fmt, args);
    SYSTEMTIME t;
    GetLocalTime(&t);
    fwprintf(stream, L"%02d-%02d %02d:%02d:%02d [%c] %s\n", t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, level, message);
    fflush(stream);
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
