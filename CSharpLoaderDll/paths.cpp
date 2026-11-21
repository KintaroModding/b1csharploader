#include "pch.h"
#include "paths.hpp"
#include <vector>

namespace {
    std::wstring find_game_dir() {
        HMODULE module = nullptr;
        // the module that contains this function: version.dll or hid.dll
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&find_game_dir), &module)) {
            return L".";
        }
        std::vector<wchar_t> buffer(MAX_PATH);
        for (;;) {
            const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0) {
                return L".";
            }
            if (length < buffer.size()) {
                std::wstring path(buffer.data(), length);
                const size_t slash = path.find_last_of(L"\\/");
                return slash == std::wstring::npos ? L"." : path.substr(0, slash);
            }
            buffer.resize(buffer.size() * 2);
        }
    }
}

const std::wstring& game_dir() {
    static const std::wstring dir = find_game_dir();
    return dir;
}

std::wstring loader_path(const wchar_t* name) {
    std::wstring path = game_dir() + L"\\CSharpLoader";
    if (name != nullptr) {
        path += L"\\";
        path += name;
    }
    return path;
}
