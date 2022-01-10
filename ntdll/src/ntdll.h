#pragma once

#include <stdint.h>
#include <string_view>
#include <span>

// string.cpp
size_t utf8_to_utf16_len(std::string_view sv);
void utf8_to_utf16(std::span<char16_t> buf, std::string_view sv);
std::span<char> sp_itoa_hex(std::span<char> dest, uint64_t num, unsigned int padding);
