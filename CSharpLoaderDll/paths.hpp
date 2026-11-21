#pragma once
#include <string>

// Full paths from the folder of this dll (the game's Win64 folder), so they do not depend on the working folder.

// the folder of this dll, without a trailing backslash
const std::wstring& game_dir();

// the CSharpLoader folder, or a file or folder in it when name is given
std::wstring loader_path(const wchar_t* name = nullptr);
