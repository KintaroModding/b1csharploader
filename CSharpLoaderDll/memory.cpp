#include "pch.h"
#include "memory.h"
#include "log.hpp"
#include <cstring>

namespace {
	// -1 is a wildcard
	std::vector<int> pattern_to_bytes(const char* pattern)
	{
		std::vector<int> bytes;
		const char* end = pattern + std::strlen(pattern);
		for (char* current = const_cast<char*>(pattern); current < end; ++current)
		{
			if (*current == '?')
			{
				++current;
				if (*current == '?')
					++current;
				bytes.push_back(-1);
			}
			else if (*current != ' ')
			{
				bytes.push_back(static_cast<int>(std::strtoul(current, &current, 16)));
			}
		}
		return bytes;
	}

	// bytes that are everywhere in x64 code, a bad choice for the byte that memchr looks for
	bool is_common_byte(int value)
	{
		switch (value)
		{
		case 0x00: case 0x48: case 0x8B: case 0x89: case 0xFF: case 0xCC: case 0x24: case 0x4C: case 0x8D: case 0x44:
			return true;
		}
		return false;
	}

	// the index of the byte that memchr looks for, -1 when the pattern has only wildcards
	int anchor_index(const std::vector<int>& pattern)
	{
		int anchor = -1;
		for (size_t i = 0; i < pattern.size(); ++i)
		{
			if (pattern[i] < 0)
				continue;
			if (anchor < 0)
				anchor = static_cast<int>(i);
			if (!is_common_byte(pattern[i]))
				return static_cast<int>(i);
		}
		return anchor;
	}

	bool matches_at(const std::uint8_t* at, const std::vector<int>& pattern)
	{
		for (size_t j = 0; j < pattern.size(); ++j)
		{
			if (pattern[j] >= 0 && at[j] != pattern[j])
				return false;
		}
		return true;
	}

	// returns the first match in [begin, end), and adds the number of all matches to count
	std::uint8_t* scan(std::uint8_t* begin, std::uint8_t* end, const std::vector<int>& pattern, int anchor, std::size_t& count)
	{
		const size_t size = pattern.size();
		if (size == 0 || anchor < 0 || end < begin || static_cast<size_t>(end - begin) < size)
			return nullptr;
		std::uint8_t* first = nullptr;
		std::uint8_t* const lastStart = end - size;
		// the anchor byte of the first and the last possible start
		std::uint8_t* search = begin + anchor;
		std::uint8_t* const searchEnd = lastStart + anchor + 1;
		while (search < searchEnd)
		{
			auto* hit = static_cast<std::uint8_t*>(std::memchr(search, pattern[anchor], static_cast<size_t>(searchEnd - search)));
			if (hit == nullptr)
				break;
			std::uint8_t* start = hit - anchor;
			if (matches_at(start, pattern))
			{
				if (first == nullptr)
					first = start;
				++count;
			}
			search = hit + 1;
		}
		return first;
	}
}

std::uint8_t* find_pattern(std::uint8_t* base, const std::string& byte_array, std::size_t* count)
{
	std::size_t matches = 0;
	const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
	const auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos_header->e_lfanew);
	const size_t size_of_image = nt_headers->OptionalHeader.SizeOfImage;
	const std::vector<int> pattern = pattern_to_bytes(byte_array.c_str());
	const int anchor = anchor_index(pattern);

	// code is in the executable sections, scanning only them is faster and finds fewer false matches
	std::uint8_t* found = nullptr;
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt_headers);
	for (WORD i = 0; i < nt_headers->FileHeader.NumberOfSections; ++i, ++section)
	{
		if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0 || section->VirtualAddress >= size_of_image)
			continue;
		size_t size = section->Misc.VirtualSize != 0 ? section->Misc.VirtualSize : section->SizeOfRawData;
		size = (std::min)(size, size_of_image - section->VirtualAddress);
		std::uint8_t* begin = base + section->VirtualAddress;
		std::uint8_t* first = scan(begin, begin + size, pattern, anchor, matches);
		if (found == nullptr)
			found = first;
	}
	if (found == nullptr)
	{
		matches = 0;
		found = scan(base, base + size_of_image, pattern, anchor, matches);
	}
	if (count != nullptr)
		*count = matches;
	return found;
}

std::uint8_t* find_sig(LPCSTR module_name, const std::string& byte_array)
{
	HMODULE module = GetModuleHandleA(module_name);
	if (!module)
		return nullptr;
	std::size_t count = 0;
	std::uint8_t* found = find_pattern(reinterpret_cast<std::uint8_t*>(module), byte_array, &count);
	if (count > 1)
	{
		// after a game update, a pattern that is not unique can find the wrong code
		log_warn("signature matches %zu times, the first one is used: %s", count, byte_array.c_str());
	}
	return found;
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

	if (this->pointer != 0)
		this->pointer += value;
	return *this;
}

signature signature::sub(uint32_t value)
{
	if (!this->imported)
		*this = this->import();

	if (this->pointer != 0)
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

	// reading at 0 + offset would crash the game
	if (this->pointer != 0)
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
