#include "pch.h"
#include "memory.h"


std::uint8_t* find_sig(LPCSTR module_name, const std::string& byte_array)
{
	HMODULE module = GetModuleHandleA(module_name);

	if (!module)
		return nullptr;

	static const auto pattern_to_byte = [](const char* pattern)
	{
		auto bytes = std::vector<int>{};
		const auto start = const_cast<char*>(pattern);
		const auto end = const_cast<char*>(pattern) + std::strlen(pattern);

		for (auto current = start; current < end; ++current)
		{
			if (*current == '?')
			{
				++current;

				if (*current == '?')
					++current;

				bytes.push_back(-1);
			}
			else
			{
				bytes.push_back(std::strtoul(current, &current, 16));
			}
		}
		return bytes;
	};

	const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
	const auto nt_headers =
		reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<std::uint8_t*>(module) + dos_header->e_lfanew);

	const auto size_of_image = nt_headers->OptionalHeader.SizeOfImage;
	const auto pattern_bytes = pattern_to_byte(byte_array.c_str());
	const auto scan_bytes = reinterpret_cast<std::uint8_t*>(module);

	const auto pattern_size = pattern_bytes.size();
	const auto pattern_data = pattern_bytes.data();

	for (auto i = 0ul; i < size_of_image - pattern_size; ++i)
	{
		auto found = true;

		for (auto j = 0ul; j < pattern_size; ++j)
		{
			if (scan_bytes[i + j] == pattern_data[j] || pattern_data[j] == -1)
				continue;
			found = false;
			break;
		}
		if (!found)
			continue;
		return &scan_bytes[i];
	}

	return nullptr;
}

signature::signature(const std::string& sig)
{
	this->imported = false;
	this->sig = sig;
	this->pointer = 0;
}

signature signature::import(const char* module_name)
{
	this->imported = true;
	this->pointer = (uint64_t)find_sig(module_name, this->sig);
	return *this;
}

signature signature::add(uint32_t value)
{
	if (!this->imported)
		*this = this->import();

	this->pointer += value;
	return *this;
}

signature signature::sub(uint32_t value)
{
	if (!this->imported)
		*this = this->import();

	this->pointer -= value;
	return *this;
}

signature signature::dump(const std::string& func_name)
{
	if (!this->imported)
		*this = this->import();

	return *this;
}

signature signature::instruction(uint32_t offset)
{
	if (!this->imported)
		*this = this->import();

	this->pointer = *(int*)(this->pointer + offset) + this->pointer;
	return *this;
}

#ifdef _WIN64
uint64_t signature::GetPointer()
{
	if (!this->imported)
		*this = this->import();
	return this->pointer;
}
#else
uint32_t signature::GetPointer()
{
	if (!this->imported)
		*this = this->import();
	return this->pointer;
}
#endif