#pragma once

#include <string_view>
#include <span>
#include <stdint.h>

// shell.cpp
void print_hex(uint64_t num, unsigned int padding = 0);

// ls.cpp
void ls(std::string_view param);

// string.cpp
size_t utf16_to_utf8_len(std::u16string_view u16sv);
void utf16_to_utf8(std::span<char> buf, std::u16string_view u16sv);
size_t utf8_to_utf16_len(std::string_view sv);
void utf8_to_utf16(std::span<char16_t> buf, std::string_view sv);
void print(std::string_view s);
