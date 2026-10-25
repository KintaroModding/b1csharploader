#include "pch.h"
#include "log.hpp"
#include "overlay.hpp"
#include <algorithm>
#include <iterator>
#include <mutex>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <vector>

namespace {
    constexpr size_t KeepLogFiles = 5;
    const wchar_t LogDir[] = L"CSharpLoader\\Logs";

    std::mutex fileMutex;
    HANDLE logFile = INVALID_HANDLE_VALUE;  // guarded by fileMutex
    bool logFileOpened = false;             // guarded by fileMutex

    // log files are named like "2026-10-25_10-23-35.log", so they sort by time
    bool is_log_file_name(const std::wstring& name) {
        return name.size() == 23 && name[4] == L'-' && name[10] == L'_' && name.compare(19, 4, L".log") == 0;
    }

    // keeps the newest log files, so there are KeepLogFiles with the new one
    void delete_old_log_files() {
        std::vector<std::wstring> names;
        WIN32_FIND_DATAW wfd;
        HANDLE find = FindFirstFileW((std::wstring(LogDir) + L"\\*.log").c_str(), &wfd);
        if (find == INVALID_HANDLE_VALUE) {
            return;
        }
        do {
            if (is_log_file_name(wfd.cFileName)) {
                names.push_back(wfd.cFileName);
            }
        } while (FindNextFileW(find, &wfd));
        FindClose(find);
        std::sort(names.begin(), names.end());
        while (names.size() >= KeepLogFiles) {
            DeleteFileW((std::wstring(LogDir) + L"\\" + names.front()).c_str());
            names.erase(names.begin());
        }
    }

    void open_log_file() {
        CreateDirectoryW(L"CSharpLoader", nullptr);
        CreateDirectoryW(LogDir, nullptr);
        delete_old_log_files();
        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t path[MAX_PATH];
        swprintf_s(path, L"%s\\%04d-%02d-%02d_%02d-%02d-%02d.log", LogDir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        logFile = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    // written right away without a buffer, so the file has every line even when the game crashes
    void write_to_file(const wchar_t* line) {
        const int size = WideCharToMultiByte(CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) {
            return;
        }
        std::string text(static_cast<size_t>(size - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, line, -1, &text[0], size, nullptr, nullptr);
        text += "\r\n";

        std::lock_guard<std::mutex> lock(fileMutex);
        if (!logFileOpened) {
            logFileOpened = true;
            open_log_file();
        }
        if (logFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(logFile, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        }
    }

    void write_line(FILE* stream, char level, const char* fmt, va_list args) {
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
            log_line(wideLine);
        }
    }

    void write_line(FILE* stream, wchar_t level, const wchar_t* fmt, va_list args) {
        wchar_t message[1024];
        _vsnwprintf_s(message, _TRUNCATE, fmt, args);
        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t line[1100];
        _snwprintf_s(line, _TRUNCATE, L"%02d-%02d %02d:%02d:%02d [%c] %s", t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, level, message);
        fwprintf(stream, L"%s\n", line);
        fflush(stream);
        log_line(line);
    }
}

void log_line(const wchar_t* line) {
    write_to_file(line);
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

extern "C" __declspec(dllexport) void CSharpLoader_LogLine(const wchar_t* line) {
    if (line != nullptr) {
        log_line(line);
    }
}
