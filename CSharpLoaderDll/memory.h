#pragma once
#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Finds a byte pattern like "48 8B 05 ? ? ? ?" in the image at base, '?' matches any byte.
// It scans the executable sections first, and the whole image when they do not have it.
// count gets the number of matches in the scanned range, more than 1 means the pattern is not unique.
std::uint8_t* find_pattern(std::uint8_t* base, const std::string& pattern, std::size_t* count);

// find_pattern in a loaded module, nullptr is the game exe. Logs a warning when the pattern is not unique.
std::uint8_t* find_sig(LPCSTR module_name, const std::string& byte_array);

class signature
{
public:

	signature(const std::string& sig);

	signature import(const char* module_name = NULL);

	// these keep 0 when the pattern was not found
	signature add(uint32_t offset);
	signature sub(uint32_t offset);
	signature instruction(uint32_t offset);

	signature dump(const std::string& func_name);

#ifdef _WIN64
	uint64_t GetPointer();
#else
	uint32_t GetPointer();
#endif

private:
#ifdef _WIN64
	uint64_t pointer;
#else
	uint32_t pointer;
#endif

	bool imported;
	std::string sig;

};
