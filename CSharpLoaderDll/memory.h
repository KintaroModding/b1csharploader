#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

std::uint8_t* find_sig(LPCSTR module_name, const std::string& byte_array);

class signature
{
public:

	signature(const std::string& sig);

	signature import(const char* module_name = NULL);

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
